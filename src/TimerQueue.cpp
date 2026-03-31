#include <sys/timerfd.h>
#include <unistd.h>
#include "TimerQueue.h"
#include "EventLoop.h"
#include "Channel.h"

TimerQueue::TimerQueue(EventLoop* loop){
    loop_=loop;
    timerfd_=timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
    timerfdChannel_=std::make_unique<Channel>(loop_,timerfd_);
    timerfdChannel_->setReadCallback(std::bind(&TimerQueue::handleRead,this));
    timerfdChannel_->enableReading();
}
TimerQueue::~TimerQueue(){
    ::close(timerfd_);
    timers_.clear();
    activeTimers_.clear();
    cancelingTimers_.clear();
    timersOwned_.clear();

}
TimerId TimerQueue::addTimer(TimerCallback cb,TimePoint when,Duration interval){
    auto timer=std::make_unique<Timer>(std::move(cb),when,interval);
    uint64_t sequence=timer->sequence();
    TimerId id(sequence,loop_);
    if(loop_->isInLoopThread()){
        addTimerInLoop(std::move(timer));
    }
    else{
        loop_->runInLoop([this,timer=std::move(timer)]()mutable{
            addTimerInLoop(std::move(timer));
        });
    }
    return id;
}

void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> timer){
    if(pendingCancel_.count({timer.get(),timer->sequence()})){
        //如果这个timer还未入队就被cancel了，直接忽略
        pendingCancel_.erase({timer.get(),timer->sequence()});
        return;
    }
    //判断是否早于当前最早到期
    bool earliestChanged=timers_.empty()||timer->expiration()<timers_.begin()->first;
    timers_.insert({timer->expiration(),timer.get()});
    activeTimers_.insert({timer.get(),timer->sequence()});
    timersOwned_[timer->sequence()]=std::move(timer);
    if(earliestChanged){
        resetTimerfd(timer->expiration());
    }
}

void TimerQueue::cancel(TimerId id){
    if(loop_->isInLoopThread()){
        cancelInLoop(id);
    }
    else{
        loop_->runInLoop([this,id](){
            cancelInLoop(id);
        });
    }
}

void TimerQueue::cancelInLoop(TimerId id){
    
}