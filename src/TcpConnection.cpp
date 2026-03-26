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
            const char* eol=inputBuffer_.findEOL();
            while(eol){
                std::string msg(inputBuffer_.peek(),eol-inputBuffer_.peek());
                if(messageCallback_)
                    messageCallback_(shared_from_this(),msg);

                inputBuffer_.retrieveUntil(eol+1);
                eol=inputBuffer_.findEOL();
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
        ssize_t n=outputBuffer_.writeFd(fd_,nullptr);
        if(n>0){//发送成功，清除outputBuffer_中字节
            
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
void TcpConnection::send(const std::string &msg){//
    if(loop_->isInLoopThread()){//如果在IO线程中，直接发送
        sendInLoop(msg);
    }
    else{//否则转发到IO线程执行发送
        loop_->runInLoop([this,msg](){
            sendInLoop(msg);
        });
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
}

void TcpConnection::handleError(){
    int err = 0;
    socklen_t len = sizeof(err);

    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    {
        err = errno;
    }
    std::cerr << "TcpConnection error: " << strerror(err) << std::endl;
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
void TcpConnection::sendInLoop(const std::string& msg){

    auto line=msg+"\n";
    if(outputBuffer_.readableBytes()==0){//如果outputBuffer_为空，尝试直接发送
        ssize_t n=::write(fd_,line.data(),line.size());

        if(n>=0){
            if((size_t)n<line.size()){//未全部发送完，剩余数据存入outputBuffer
            outputBuffer_.append(line.data()+n,line.size()-n);
            channel_->enableWriting();
        }

        }
        else{
            if(errno==EAGAIN||errno==EWOULDBLOCK)
                n=0;
            else
                throw::std::runtime_error("write() failed");
            outputBuffer_.append(line.data()+n,line.size()-n);
            channel_->enableWriting();
        }
        
    }
    else{//outputBuffer_不为空，直接追加到outputBuffer_后面
        outputBuffer_.append(line.data(),line.size());
        channel_->enableWriting();
    }
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
    loop_->addChannel(channel_);
}

void TcpConnection::connectionDestroyed(){
    if(connection_){
        connection_=false;
        channel_->disableAll();
        loop_->removeChannel(fd_);
        delete channel_;
    }
}