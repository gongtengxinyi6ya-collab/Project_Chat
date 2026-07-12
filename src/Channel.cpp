#include "Channel.h"
#include "EventLoop.h"
#include "logger/LogMacros.h"
Channel::Channel(EventLoop* loop,int fd):fd_(fd),events_(0),revents_(0),inEpoll_(false),loop_(loop)
{

}

int Channel::fd() const{
    return fd_;
}

void Channel::setReadCallback(EventCallback cb)
{
    readCallback_=std::move(cb);
}

void Channel::setWriteCallback(EventCallback cb)
{
    writeCallback_=std::move(cb);

}
void Channel::setCloseCallback(EventCallback cb)
{
    closeCallback_=std::move(cb);
}

void Channel::setErrorCallback(EventCallback cb){
    errorCallback_=std::move(cb);
}
void Channel::handleEvent()noexcept{
    auto invokeSafely=[this](const char* callbackName,const std::function<void()>& callback)noexcept->bool{
        if(!callback){
            return true;
        }
        try{
            callback();
            return true;
        }catch (const std::exception& e) {
            LOG_ERROR(std::string("Channel callback threw exception, fd=") +std::to_string(fd_) +", callback=" + callbackName +", error=" + e.what() );
        } catch (...) {
            LOG_ERROR(std::string("Channel callback threw unknown exception, fd=") +std::to_string(fd_) +", callback=" + callbackName);
        }
        return false;
    };
    if(revents_&EPOLLHUP&&!(revents_&EPOLLIN)){
        invokeSafely("close",closeCallback_);
        return;
    }
    if (revents_ & EPOLLERR) {
        invokeSafely("error", errorCallback_);
        return;
    }
    if (revents_ & EPOLLIN) {
        if (!invokeSafely("read", readCallback_)) {
            invokeSafely("error",errorCallback_);
            return;
        }
    }
    if (revents_ & EPOLLOUT) {
        if(!invokeSafely("write", writeCallback_)){
            invokeSafely("error",errorCallback_);
        }
    }
}

void Channel::setRevents(int revent){
    revents_=std::move(revent);
}
bool Channel::enableReading()noexcept{
    events_|=EPOLLIN;
    return loop_->updateChannel(this);
}

bool Channel::enableWriting()noexcept{
    if((events_&EPOLLOUT)!=0){//已经监听EPOLLOUT，直接返回
        return true;
    }
    events_|=EPOLLOUT;
    if (!loop_->updateChannel(this)) {
        events_ &= ~EPOLLOUT;
        return false;
    }

    return true;
}

bool Channel::disableWriting()noexcept{
    if((events_&EPOLLOUT)==0){//本来没有监听
        return true;
    }
    events_&=~EPOLLOUT;
    if (!loop_->updateChannel(this)) {
        events_ |= EPOLLOUT;
        return false;
    }

    return true;
}

void Channel::disableAll()noexcept{
    events_=0;
}
bool Channel::remove()noexcept{
    return loop_->removeChannel(this);
}

int Channel::events() const{
    return events_;
}

bool Channel::inEpoll() const{
    return inEpoll_;
}

void Channel::setInEpoll(bool inEpoll){
    inEpoll_=inEpoll;
}