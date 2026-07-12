#include "infra/signal/SignalHandler.h"
#include "EventLoop.h"
#include "Channel.h"
#include "logger/LogMacros.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace infra::signal{
std::atomic<int> SignalHandler::receivedSignal_{0};
int SignalHandler::notifyFd_{-1};


SignalHandler::SignalHandler(EventLoop* loop)
:loop_(loop){
    eventFd_=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(eventFd_==-1){
        throw std::runtime_error("eventfd failed");
    }
    channel_=std::make_unique<Channel>(loop_,eventFd_);
    channel_->setReadCallback([this]{handleRead();});
}
SignalHandler::~SignalHandler(){
    if(channel_){
        channel_->disableAll();
        if(channel_->inEpoll()){
            loop_->removeChannel(channel_.get());
        }
    }
    if(eventFd_>=0){
        ::close(eventFd_);
    }
    if(notifyFd_==eventFd_){
        notifyFd_=-1;
    }
}

void SignalHandler::setSignalCallback(SignalCallback cb){
    signalCallback_=std::move(cb);
}
void SignalHandler::start(){
    notifyFd_=eventFd_;
    struct sigaction sa{};
    sa.sa_handler=&SignalHandler::handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    if(::sigaction(SIGINT,&sa,nullptr)<0){
        throw std::runtime_error("sigaction SIGINT failed");
    }
    if(::sigaction(SIGTERM,&sa,nullptr)<0){
        throw std::runtime_error("sigaction SIGTERM failed");
    }
    if(channel_&&!channel_->enableReading()){
        throw std::runtime_error("Failed to enableReading");
    }
    
}
void SignalHandler::handleRead(){
    //从evnetFd读取uint64
    uint64_t one=0;
    if(::read(eventFd_,&one,sizeof(one))!=sizeof(one)){
        return ;
    }
    int signo=receivedSignal_.exchange(0,std::memory_order_relaxed);
    if(signo!=0&&signalCallback_){
        signalCallback_(signo);
    }
}
void SignalHandler::handleSignal(int signo){
    receivedSignal_.store(signo,std::memory_order_relaxed);
    if(notifyFd_>=0){
        uint64_t one=1;
        ::write(notifyFd_,&one,sizeof(one));
    }
}
}