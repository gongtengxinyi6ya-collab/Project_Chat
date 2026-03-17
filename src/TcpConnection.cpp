#include "TcpConnection.h"

TcpConnection::TcpConnection(EventLoop* loop,int fd)
:loop_(loop),fd_(fd),connection_(true){
    channel_=new Channel(loop_,fd_);
    //绑定回调
    channel_->setReadCallback([this](){handleRead();});
    channel_->setWriteCallback([this](){handleWrite();});
    channel_->setCloseCallback([this](){handleClose();});
    channel_->setErrorCallback([this](){handleError();});


    channel_->enableReading();//开启读事件
    loop_->addChannel(channel_);
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
            inputBuffer_.append(buffer,n);
            if(messageCallback_)
                messageCallback_(fd_,inputBuffer_);
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

    closeCallback_(fd);
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