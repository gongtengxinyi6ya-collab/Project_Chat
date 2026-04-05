#include "TcpConnection.h"
#include "TcpServer.h"
#include "ThreadPool.h"

TcpConnection::TcpConnection(EventLoop* loop,int fd,ThreadPool* threadPool,TcpServer* server)
:loop_(loop),fd_(fd),threadPool_(threadPool),server_(server),connection_(true){

}

TcpConnection::~TcpConnection(){
    int fd = fd_;
    if(fd >= 0){
        loop_->removeChannel(fd);
        ::close(fd);
        fd_ = -1;
    }
    delete channel_;
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
                if(len>kMaxFrameLen){//超过长度认为非法协议
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
        ssize_t n=::write(fd_,outputBuffer_.peek(),outputBuffer_.readableBytes());
        if(n>0){//发送成功，清除outputBuffer_中字节
            outputBuffer_.retrieve(n);
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
    if(!connection_)
        return;
    connection_=false;
    channel_->disableAll();

    int fd = fd_;
    if(fd_ >= 0){
        loop_->removeChannel(fd_);
        ::close(fd_);
        fd_ = -1;
    }

    closeCallback_(shared_from_this());
    stopHeartbeat();
    if(idleTimerId_.valid()){
        loop_->cancel(idleTimerId_);
    }
}

void TcpConnection::handleError(){
    int err = 0;
    socklen_t len = sizeof(err);

    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    {
        err = errno;
    }
    LOG_ERROR("TcpConnection::handleError");
}


void TcpConnection::setCloseCallback(CloseCallback cb){
    closeCallback_=std::move(cb);
}

void TcpConnection::setMessageCallback(MessageCallback cb){
    messageCallback_=std::move(cb);
}
EventLoop* TcpConnection::getLoop() const{
    return loop_;
}
int TcpConnection::fd() const{
    return fd_;
}

void TcpConnection::send(const std::string &msg){//
    if(loop_->isInLoopThread()){//如果在IO线程中，直接发送
        sendInLoop(msg);
    }
    else{//否则转发到IO线程执行发送
        loop_->runInLoop([self=shared_from_this(),msg](){
            self->sendInLoop(msg);
        });
    }
}
void TcpConnection::sendInLoop(const std::string& msg){

    uint32_t len=msg.size();
    if(len>kMaxFrameLen){
        std::cerr<<"Message too long to send"<<std::endl;
        return;
    }
    
    outputBuffer_.appendUint32(len);
    outputBuffer_.append(msg.data(),len);
    channel_->enableWriting();//注册写事件，等待发送机会
}
//连接建立，创建Channel，绑定回调，注册到EventLoop
void TcpConnection::connectionEstablished(){
    channel_=new Channel(loop_,fd_);
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
    if(connection_){
        connection_=false;
        channel_->disableAll();
        loop_->removeChannel(fd_);
        delete channel_;
    }
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
    lastHeartbeeatTime_=std::chrono::steady_clock::now();
    if(payload=="PONG"){
        lastPong_=std::chrono::steady_clock::now();
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
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now-lastPong_)>heartbeatTimeout_){
        LOG_WARN("Connection " + std::to_string(fd_) + " heartbeat timeout, no PONG received for " + std::to_string(heartbeatTimeout_.count()) + " ms, closing connection");
        handleClose();
    }
    else if(std::chrono::duration_cast<std::chrono::milliseconds>(now-lastHeartbeeatTime_)>heartbeatInterval_){
        LOG_INFO("No data received from connection " + std::to_string(fd_) + " for a while, sending PING");
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