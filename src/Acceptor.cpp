#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include "logger/LogMacros.h"
Acceptor::Acceptor(EventLoop* loop,std::string host,uint16_t port ,int backlog ,bool tcpNoDelay,bool keepAlive)
:loop_(loop),host_(host),port_(port),backlog_(backlog),tcpNoDelay_(tcpNoDelay),keepAlive_(keepAlive)
{   
    if(!loop_){
        throw std::invalid_argument("loop invalid");
    }
    idleFd_=::open("/dev/null",O_RDONLY|O_CLOEXEC);
    if(idleFd_<0){
        throw std::runtime_error("failed to open /dev/null");
    }
}
Acceptor::~Acceptor(){
    if(listening_){
        if(loop_->isInLoopThread()){
            if(channel_){
                channel_->disableAll();
                loop_->removeChannel(channel_.get());
                listening_=false;
            }
        }
        else{
            LOG_ERROR("acceptor is listening");
        }
    }
    if(idleFd_>=0){
        ::close(idleFd_);
        idleFd_=-1;
    }
}

bool Acceptor::configureClientSocket(int fd) noexcept{
    if (fd < 0) {
        return false;
    }
    const bool noDelayOk =Socket::setTcpNoDelay(fd, tcpNoDelay_);
    const bool keepAliveOk =Socket::setKeepAlive(fd, keepAlive_);
    return noDelayOk && keepAliveOk;
}
void Acceptor::handleRead(){
    while(true){
        int savedErrno{0};
        int clientFd=listenSocket_.accept(&savedErrno);

        if(clientFd>=0){
            if(!configureClientSocket(clientFd)){
                LOG_WARN("Failed to set ClientSocket config");
            }
            
            if(newConnectionCallback_){
                newConnectionCallback_(clientFd);
            }
        }
        else{
            if(savedErrno==EAGAIN||savedErrno==EWOULDBLOCK){//ET模式下连接已经全部取完
                break;
            }
            else if(savedErrno==EINTR){//被信号中断
                continue;
            }
            else if(savedErrno==ECONNABORTED||savedErrno==EPROTO){//该连接accept前已经异常
                LOG_WARN("connection Error");
                continue;
            }
            else if(savedErrno==EMFILE||savedErrno==ENFILE){
                handleFdExhaustion();
                break;
            }
            else{//其他错误
                LOG_WARN("accept error:"+std::to_string(savedErrno));
                break;
            }
        }
        
    }
}
void Acceptor::handleFdExhaustion()noexcept{
    if(idleFd_>=0){
    ::close(idleFd_);//关闭idleFd_,腾出fd
    idleFd_=-1;
    }
    int connFd=::accept(listenSocket_.fd(),nullptr,nullptr);
    if(connFd>=0){
        ::close(connFd);
    }
    idleFd_=::open("/dev/null",O_RDONLY|O_CLOEXEC);
}
void Acceptor::setNewConnectionCallback(NewConnectionCallback cb){
    newConnectionCallback_=std::move(cb);
}

void Acceptor::listen(){
    if(listening_){
        return ;
    }
    listenSocket_.bind(host_,port_);
    listenSocket_.listen(backlog_);

    channel_=std::make_unique<Channel>(loop_,listenSocket_.fd());
    channel_->setReadCallback(std::bind(&Acceptor::handleRead,this));
    if(!channel_->enableReading()){
        throw std::runtime_error("Failed to enableReading");
    }
    listening_=true;
}

void Acceptor::stop(){
    if(!listening_){
        return;
    }
    if(!loop_->isInLoopThread()){
        loop_->runInLoop([this](){
            channel_->disableAll();
            loop_->removeChannel(channel_.get());
            listening_=false;
        });
    }
    else{
        channel_->disableAll();
        loop_->removeChannel(channel_.get());
        listening_=false;
    }

}