#include "TcpConnection.h"
#include "TcpServer.h"
#include "ThreadPool.h"

TcpConnection::TcpConnection(EventLoop* loop,int fd,ThreadPool* threadPool,TcpServer* server,const AppConfig& config)
:loop_(loop),fd_(fd),threadPool_(threadPool),server_(server),connected_(true),heartbeatInterval_(config.net().heartBeatMs),heartbeatTimeout_(config.net().heartbeatTimeoutMs),idleTimeout_(config.net().idleTimeoutMs),maxFrameLen(config.net().maxFrameLen),highWaterMark_(config.net().connHighWaterMark),lowWaterMark_(config.net().connLowWaterMark),hardLimit_(config.net().connHardLimit),maxOverloadDropCount_(config.net().maxOverloadDropCount){
}

TcpConnection::~TcpConnection(){
    closeFd();
}

void TcpConnection::handleRead(){
    int saveErrno;
    while(true){

        ssize_t n=inputBuffer_.readFd(fd_,&saveErrno);
        if(n>0){//当n>0,可能还要数据继续循环
            //解析消息，调用消息回调
            while(true){
                if(inputBuffer_.readableBytes()<4)
                    break;

                uint32_t len=inputBuffer_.peekUInt32();
                if(len==0){//空消息
                    inputBuffer_.retrieveUInt32();
                    if(messageCallback_)
                        messageCallback_(shared_from_this(),"");
                    continue;   
                }
                if(len>maxFrameLen){//超过长度认为非法协议
                    handleClose();
                    return; 
                }
                if(inputBuffer_.readableBytes()<4+len){
                    //长度不够
                    break;
                }
                inputBuffer_.retrieveUInt32();
                auto payload=inputBuffer_.retrieveAsString(len);
                if(handleControlFrame(payload)){//如果是控制帧已处理，继续读取下一条消息
                    continue;
                }
                lastActiveTime_=std::chrono::steady_clock::now();
                refreshIdleTimer();
                if(messageCallback_)
                    messageCallback_(shared_from_this(),payload);
            }
        }
        else if(n==0){//客户端关闭连接
            handleClose();
            return;
        }
        else {
            if(saveErrno==EAGAIN||saveErrno==EWOULDBLOCK)//数据读完
                break;
            else if(saveErrno==EINTR)//信号打断
                continue;
            else{
                handleError();
                return;
            }

        }
    }
}


void TcpConnection::handleWrite(){
    while(outputBuffer_.readableBytes()>0){
        ssize_t n=::write(fd_,outputBuffer_.peek(),outputBuffer_.readableBytes());//发送数据
        if(n>0){//发送成功，清除outputBuffer_中字节
            outputBuffer_.retrieve(n);
            updatePendingEstimate();//每次成功发送并清除后，同步更新估算值
            size_t pending=pendingBytesEstimate_.load(std::memory_order_relaxed);
            size_t overload=overloaded_.load(std::memory_order_relaxed);
            if(overload&&pending<lowWaterMark_){//如果之前过载且已降到低水位，记录日志并恢复正常状态
            overloaded_.store(false,std::memory_order_relaxed);
            overloadDropCount_.store(0,std::memory_order_relaxed);//清空连续过载丢弃数
            if(lowWaterCallback_){
                lowWaterCallback_(shared_from_this(),pendingBytes());
            }
            LOG_INFO("Connection " + std::to_string(fd_) + " is recovered, pending bytes: " + std::to_string(pendingBytes()));
        }
        }
        else if(n==-1){
        if(errno==EAGAIN||errno==EWOULDBLOCK)//缓冲区满需等待下一次
            return;
        else{
            handleError();
            return;
        }
    }
    //如果outputBuffer_为空，关闭写事件
    if(outputBuffer_.readableBytes()==0){
        channel_->disableWritng();
    }   
    
}
}

void TcpConnection::handleClose(){
    if(!connected_.exchange(false)){//防止重复调用closeCallback_，记录日志并
        return;
    }
    stopHeartbeat();
    if(idleTimerId_.valid()){
        loop_->cancel(idleTimerId_);
    }
    channel_->disableAll();
    loop_->removeChannel(fd_);
    closeCallback_(shared_from_this());
}

void TcpConnection::handleError(){
    int err = 0;
    socklen_t len = sizeof(err);

    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    {
        err = errno;
    }
    LOG_ERROR("TcpConnection::handleError"+std::to_string(fd_)+", error: "+std::to_string(err)+", "+strerror(err));
    handleClose();//发生错误直接关闭连接
}


void TcpConnection::setCloseCallback(CloseCallback cb){
    closeCallback_=std::move(cb);
}

void TcpConnection::setMessageCallback(MessageCallback cb){
    messageCallback_=std::move(cb);
}
void TcpConnection::setHighWaterCallback(HighWaterCallback cb){
    highWaterCallback_=std::move(cb);
}
void TcpConnection::setLowWaterCallback(LowWaterCallback cb){
    lowWaterCallback_=std::move(cb);
}
EventLoop* TcpConnection::getLoop() const{
    return loop_;
}
int TcpConnection::fd() const{
    return fd_;
}

void TcpConnection::send(const std::string &msg){//
    if(!connected_.load(std::memory_order_relaxed)){//防止对已关闭的fd发送数据，记录日志并返回
        return;
    }
    if(loop_->isInLoopThread()){//如果在IO线程中，直接发送
        sendInLoop(msg);
    }
    else{//否则转发到IO线程执行发送
        std::weak_ptr<TcpConnection> weakSelf=shared_from_this();
        loop_->runInLoop([weakSelf,msg](){
            if(auto self=weakSelf.lock()){
                self->sendInLoop(msg);
            }
        });
    }
}
void TcpConnection::sendInLoop(const std::string& msg){
if(!connected_.load(std::memory_order_relaxed)||!channel_){//防止连接已关闭但广播任务还在队列里
        return;
    }
    uint32_t len=msg.size();
    if(len>maxFrameLen){
        LOG_WARN("Message length " + std::to_string(len) + " exceeds maximum frame length, message discarded"+" to send, fd="+std::to_string(fd_));
        return;
    }
    size_t frameBytes=4+len;
    if(!canAccept(frameBytes)){//如果超过硬限制直接丢弃
        droppedMessage_.fetch_add(1,std::memory_order_relaxed);
        uint32_t drops=overloadDropCount_.fetch_add(1,std::memory_order_relaxed)+1;
        if(drops>=maxOverloadDropCount_){
            LOG_ERROR("Connection " + std::to_string(fd_) + " has dropped " +std::to_string(overloadDropCount_) + " messages due to overload, exceeding max overload drop count of " + std::to_string(maxOverloadDropCount_) + ", closing connection");
            scheduleCloseInLoop();
        }
        return;
    }
    outputBuffer_.appendUint32(len);
    outputBuffer_.append(msg.data(),len);
    updatePendingEstimate();//append后更新估算值
    size_t pending=pendingBytesEstimate_.load(std::memory_order_relaxed);
    size_t overload=overloaded_.load(std::memory_order_relaxed);
    if(!overload&&pending>=highWaterMark_){//若超过高水位且之前未过载，记录日志并标记过载状态
        overloaded_.store(true,std::memory_order_relaxed);
        if(highWaterCallback_){
            highWaterCallback_(shared_from_this(),pendingBytes());
        }
        LOG_WARN("Connection " + std::to_string(fd_) + " is overloaded, pending bytes: " + std::to_string(pendingBytes()));
    }
    channel_->enableWriting();//注册写事件，等待发送机会
}
//连接建立，创建Channel，绑定回调，注册到EventLoop
void TcpConnection::connectionEstablished(){
    channel_=std::make_unique<Channel>(loop_,fd_);
    //绑定回调
    channel_->setReadCallback([this](){handleRead();});
    channel_->setWriteCallback([this](){handleWrite();});
    channel_->setCloseCallback([this](){handleClose();});
    channel_->setErrorCallback([this](){handleError();});

    channel_->enableReading();//开启读事件
    startHeartbeat();//启动心跳检测
    refreshIdleTimer();//启动空闲超时检测
}

void TcpConnection::connectionDestroyed(){
    if(connected_.load(std::memory_order_relaxed)&&channel_->inEpoll()){
        channel_->disableAll();
        loop_->removeChannel(fd_);
    }
    closeFd();
}
void TcpConnection::closeFd(){
    if(!fdClosed_&&fd_>=0){
        ::close(fd_);
        fdClosed_=true;
        fd_=-1;
    }
}
bool TcpConnection::canSend(size_t payloadBytes)const{
    size_t frameBytes=4+payloadBytes;//4+未加长度头的JSON字符串长度
    size_t pending=pendingBytesEstimate_.load(std::memory_order_relaxed);
    return connected_.load(std::memory_order_relaxed)&&pending+frameBytes<=hardLimit_;
}

//心跳检测接口
void TcpConnection::startHeartbeat(){
    lastPong_=std::chrono::steady_clock::now();
    lastActiveTime_=std::chrono::steady_clock::now();
    lastHeartbeeatTime_=std::chrono::steady_clock::now();
    std::weak_ptr<TcpConnection> weakSelf=shared_from_this();
    heartbeatTimerId_=loop_->runEvery(heartbeatInterval_,[weakSelf](){
        if(auto self=weakSelf.lock()){
            self->onHeartbeatTick();
        }  
    });
}
void TcpConnection::stopHeartbeat(){
    if(heartbeatTimerId_.valid()){
        loop_->cancel(heartbeatTimerId_);
    }
}

bool TcpConnection::handleControlFrame(const std::string& payload){
    if(payload=="PONG"){
        lastHeartbeeatTime_=std::chrono::steady_clock::now();
        LOG_DEBUG("Received PONG from connection " + std::to_string(fd_));
        return true;
    }
    if(payload=="PING"){
        send("PONG");
        return true;
    }
    return false;
}
void TcpConnection::onHeartbeatTick(){//心跳定时器回调，检查连接状态
    TimePoint now=std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now-lastHeartbeeatTime_)>heartbeatTimeout_){
        LOG_WARN("Connection " + std::to_string(fd_) + " heartbeat timeout, no PONG received for " + std::to_string(heartbeatTimeout_.count()) + " ms, closing connection");
        handleClose();
    }
    else if(std::chrono::duration_cast<std::chrono::milliseconds>(now-lastHeartbeeatTime_)>heartbeatInterval_){
        LOG_DEBUG("Connection " + std::to_string(fd_) + " heartbeat tick, sending PING");
        send("PING");
    }
}

//空闲超时接口
void TcpConnection::refreshIdleTimer(){
    if(idleTimerId_.valid()){
        loop_->cancel(idleTimerId_);
    }
    std::weak_ptr<TcpConnection> weakconn=shared_from_this();
    idleTimerId_=loop_->runAfter(idleTimeout_,[weakconn](){
        auto self=weakconn.lock();
        if(self){
            self->onIdTimerout();
        }
    });
}
void TcpConnection::onIdTimerout(){
    LOG_WARN("Connection " + std::to_string(fd_) + " idle timeout, no data received for " + std::to_string(idleTimeout_.count()) + " ms, closing connection");
    handleClose();
}

size_t TcpConnection::pendingBytes() const{
    return outputBuffer_.readableBytes();
}
bool TcpConnection::isOverloaded() const{
    return overloaded_;
}
bool TcpConnection::canAccept(size_t nextBytes) const{
    return pendingBytes()+nextBytes<=hardLimit_;//如果超过hardLimit_则拒绝接收新消息，直接丢弃
}

uint64_t TcpConnection::droppedMessage()const{
    return droppedMessage_.load(std::memory_order_relaxed);
}
uint32_t TcpConnection::overloadDropCount()const{
    return overloadDropCount_.load(std::memory_order_relaxed);
}
void TcpConnection::recordDrop([[maybe_unused]]size_t payloadBytes){
    droppedMessage_.fetch_add(1,std::memory_order_relaxed);
    size_t drops=overloadDropCount_.fetch_add(1,std::memory_order_relaxed)+1;
    bool wasOverload=overloaded_.exchange(true,std::memory_order_relaxed);
    if(!wasOverload&&highWaterCallback_){//未过载
        overloaded_.store(true,std::memory_order_relaxed);
        size_t pending=pendingBytesEstimate_.load(std::memory_order_relaxed);
        highWaterCallback_(shared_from_this(),pending);
    }
    if(drops>=maxOverloadDropCount_){
        scheduleCloseInLoop();
        LOG_WARN("Connection " + std::to_string(fd_) + " has dropped " +std::to_string(overloadDropCount_) + " messages due to overload, exceeding max overload drop count of " + std::to_string(maxOverloadDropCount_)+", closing connection");
    }
}
void TcpConnection::scheduleCloseInLoop(){
    std::weak_ptr<TcpConnection> weakptr=shared_from_this();
    loop_->runInLoop([weakptr](){
        if(auto self=weakptr.lock()){
            self->handleClose();
        }
    });
}
void TcpConnection::updatePendingEstimate(){
    pendingBytesEstimate_.store(outputBuffer_.readableBytes(),std::memory_order_relaxed);
}