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
    char buffer[BUFFERSIZE];
    while(true){

        ssize_t n=read(fd_,buffer,BUFFERSIZE);
        if(n>0){//读取成功
            std::string data(buffer,n);
            if(messageCallback_)//调用消息回调，交给服务器处理
                {
                    //将消息处理交给线程池，避免在IO线程中执行耗时操作
                    threadPool_->submit([this,data](){
                        messageCallback_(shared_from_this(),data);
                    });
                }
        }
        else if(n==0){//客户端关闭连接
            handleClose();
            return;
        }
        else {
            if(errno==EAGAIN)//数据读完
                break;
            else if(errno==EINTR)//信号打断
                continue;
            else{
                handleError();
                return;
            }

        }
    }
}


void TcpConnection::handleWrite(){
    while(!outputBuffer_.empty()){
        ssize_t n=write(fd_,outputBuffer_.data(),outputBuffer_.size());
        if(n>0){//发送成功，清除outputBuffer_中字节
            outputBuffer_.erase(0,n);
    }
        else if(n==-1){
        if(errno==EAGAIN||errno==EWOULDBLOCK)//缓冲区满需等待下一次
            return;
        else{
            handleError();
            return;
        }
    }
    //如果outputBuffer_已发送完，关闭写事件
    if(outputBuffer_.empty()){
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

    if(outputBuffer_.empty()){//如果outputBuffer_为空，尝试直接发送
        ssize_t n=::write(fd_,msg.data(),msg.size());

        if(n>=0){
            if((size_t)n<msg.size()){//未全部发送完，剩余数据存入outputBuffer
            outputBuffer_.append(msg.data()+n,msg.size()-n);
            channel_->enableWriting();
        }

        }
        else{
            if(errno==EAGAIN||errno==EWOULDBLOCK)
                n=0;
            else
                throw::std::runtime_error("write() failed");
            outputBuffer_.append(msg.data()+n,msg.size()-n);
            channel_->enableWriting();
        }
        
    }
    else{//outputBuffer_不为空，直接追加到outputBuffer_后面
        outputBuffer_.append(msg.data(),msg.size());
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