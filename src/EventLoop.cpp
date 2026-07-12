#include "EventLoop.h"
#include "until.h"
#include "Channel.h"
#include "timer/TimerQueue.h"
#include "timer/Timer.h"
#include "logger/LogMacros.h"
#include <system_error>
#include <stdexcept>
EventLoop:: EventLoop():looping(false),epollfd_(-1),activeEvents_(EPOLL_MAX_EVENTS),wakeupFd_(-1),threadId_(std::this_thread::get_id())
{
    epollfd_=epoll_create(EPOLL_MAX_EVENTS);
    if(epollfd_==-1){
        throw std::runtime_error("epoll_create failed");    
    }
    wakeupFd_=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(wakeupFd_==-1){
        throw std::runtime_error("eventfd failed");
    }
    wakeupChannel_=std::make_unique<Channel>(this,wakeupFd_);
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleWakeup,this));
    if(!wakeupChannel_->enableReading()){
        throw std::runtime_error("Channel:Failed to enableReading");
    }
    
    timerQueue_=std::make_unique<TimerQueue>(this);
}
EventLoop:: ~EventLoop(){
    timerQueue_.reset();
    if(wakeupChannel_){
        wakeupChannel_->disableAll();
        if(wakeupChannel_->inEpoll()){
            removeChannel(wakeupChannel_.get());
        }
    }

    if(wakeupFd_>=0){
        ::close(wakeupFd_);
        wakeupFd_=-1;
    }
    if(epollfd_>=0){
        ::close(epollfd_);
        epollfd_=-1;
    }
}
void EventLoop:: loop(){
    if(looping)
        return;

    looping=true;

    while(!quit_.load(std::memory_order_relaxed)){
        int event_cnt=epoll_wait(epollfd_,activeEvents_.data(),EPOLL_MAX_EVENTS,1000);
        if(event_cnt<0)
        {
            if(errno==EINTR)//信号中断
                continue;
            LOG_SYSERR("epoll_wait error");
            break;
        }
        
        for(int i=0;i<event_cnt;i++){
            int fd=activeEvents_[i].data.fd;
            auto it=channels_.find(fd);

            if(it!=channels_.end())
            {
                it->second->setRevents(activeEvents_[i].events);

                    try {
                        it->second->handleEvent();
                    } catch (const std::exception& e) {
                        LOG_ERROR(std::string("Error handling event for fd ") + std::to_string(fd) + ": " + e.what());
                        // 可以选择关闭连接或处理错误
                    }
                }
                    

    }
        doPendingFunctors();
    
}
    
}
void EventLoop::quit()
{
    quit_.store(true,std::memory_order_release);
    wakeup();
}


bool EventLoop::removeChannel(Channel* channel)noexcept
{
    assertInLoopThread();
    if(!channel){
        return false;
    }
    auto fd=channel->fd();
    auto it=channels_.find(fd);
    if(it!=channels_.end()){
        if(it->second!=channel){
            return false;
        }
        if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,it->first,nullptr)==-1){
            const int savedErrno = errno;
            const std::error_code ec(savedErrno, std::system_category());
            LOG_ERROR("epoll_ctl del: fd: "+std::to_string(it->first)+"errno: "+ec.message());
            return false;
        }
        channels_[fd]->setInEpoll(false);
        channels_.erase(it);
    }
    return true;
}

bool EventLoop::updateChannel(Channel* channel)noexcept
{
    assertInLoopThread();
    if(!channel){
        return false;
    }
    int fd=channel->fd();
    if(channel->inEpoll()){//已存在的fd，修改事件
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=channel->events()|EPOLLET;//关注channel关注的事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&ev)==-1){
            const int savedErrno = errno;
            const std::error_code ec(savedErrno, std::system_category());
            LOG_WARN("epoll_ctl add: fd: "+std::to_string(fd)+", events: "+std::to_string(ev.events)+"errno: "+ec.message());
            return false;
        }
    }
    else{
        //新添加的fd
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=channel->events()|EPOLLET;//关注channel关注的事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&ev)==-1){
            const int savedErrno = errno;
            const std::error_code ec(savedErrno, std::system_category());
            LOG_WARN("epoll_ctl mod: fd: "+std::to_string(fd)+", events: "+std::to_string(ev.events)+"errno: "+ec.message());
            return false;
        }
        channel->setInEpoll(true);
        channels_[fd]=channel;
    }
    return true;
}
void EventLoop::assertInLoopThread() const{
    if(!isInLoopThread()){
        LOG_FATAL("EventLoop thread violation");
        std::terminate();
    }
}
bool EventLoop::isInLoopThread() const{
    return threadId_==std::this_thread::get_id();
}


void EventLoop::wakeup(){
    uint64_t one=1;
    ssize_t n=write(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        LOG_WARN("EventLoop::wakeup() writes " + std::to_string(n) + " bytes instead of 8");
    }
}

void EventLoop::handleWakeup(){
    uint64_t one=1;
    ssize_t n=read(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        LOG_WARN("EventLoop::handleWakeup() reads " + std::to_string(n) + " bytes instead of 8");
    }
}
void EventLoop::doPendingFunctors(){
    std::vector<std::unique_ptr<TaskBase>> functors;
    {
        std::lock_guard lk(mutex_);
        functors.swap(pendingFunctors_);
    }
    for(const auto& func:functors){
        func->call();
    }

}

//定时器接口
TimerId EventLoop::runAt(TimePoint when,TimerCallback cb){
    return timerQueue_->addTimer(std::move(cb),when,Duration{0});
}
TimerId EventLoop::runAfter(Duration delay,TimerCallback cb){
    TimePoint when=std::chrono::steady_clock::now()+delay;
    return runAt(when,std::move(cb));
}
TimerId EventLoop::runEvery(Duration interval,TimerCallback cb){
    TimePoint when=std::chrono::steady_clock::now()+interval;
    return timerQueue_->addTimer(std::move(cb),when,interval);
}
void EventLoop::cancel(TimerId id){
    timerQueue_->cancel(id);
}