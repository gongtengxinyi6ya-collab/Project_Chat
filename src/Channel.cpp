#include "Channel.h"
#include "EventLoop.h"
Channel::Channel(EventLoop* loop,int fd):loop_(loop),fd_(fd),events_(0),revents_(0),inEpoll_(false)
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
void Channel::handleEvent(){
    if(revents_&EPOLLIN){
        readCallback_();
    }
    if(revents_&EPOLLOUT)
        writeCallback_();
    if(revents_&EPOLLHUP)
        closeCallback_();
    if(revents_&EPOLLERR)
        errorCallback_();
}

void Channel::setRevents(int revent){
    revents_=std::move(revent);
}
void Channel::enableReading(){
    events_|=EPOLLIN;
    loop_->updateChannel(this);
}

void Channel::enableWriting(){
    events_|=EPOLLOUT;
    loop_->updateChannel(this);
}

void Channel::disableWritng(){
    events_&=~EPOLLOUT;
    loop_->updateChannel(this);
}

void Channel::disableAll(){
    events_=0;
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