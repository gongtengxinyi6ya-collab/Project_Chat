#include <sys/timerfd.h>
#include <unistd.h>
#include "timer/TimerQueue.h"
#include "timer/Timer.h"
#include "EventLoop.h"
#include "Channel.h"

TimerQueue::TimerQueue(EventLoop* loop){
    loop_=loop;
    timerfd_=timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
    timerfdChannel_=std::make_unique<Channel>(loop_,timerfd_);
    timerfdChannel_->setReadCallback(std::bind(&TimerQueue::handleRead,this));
    if(!timerfdChannel_->enableReading()){
        throw std::runtime_error("Faile to enbaleReading");
    }
}
TimerQueue::~TimerQueue(){
    ::close(timerfd_);
    timers_.clear();
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
    if(pendingCancel_.count({timer->sequence()})){
        //如果这个timer还未入队就被cancel了，直接忽略
        pendingCancel_.erase({timer->sequence()});
        return;
    }
    //判断是否早于当前最早到期
    bool earliestChanged=timers_.empty()||timer->expiration()<timers_.begin()->first;
    auto sequence=timer->sequence();
    auto expiration=timer->expiration();
    timers_.insert({expiration,sequence});
    timersOwned_[sequence]=std::move(timer);
    if(earliestChanged){
        resetTimerfd(expiration);
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
    //先在timersOwned_中查找，如果找不到说明这个timer还未入队，加入pendingCancel_等待addTimerInLoop处理
    auto it=timersOwned_.find(id.sequence());
    if(it!=timersOwned_.end()){//找到了，说明这个timer已经入队了，需要从timers_中删除
        auto expiration=it->second->expiration();
        auto sequence=id.sequence();
        timers_.erase({expiration,id.sequence()});
        timersOwned_.erase(it);
        //如果正在执行到期回调，加入cancelingTimers_等待handleRead处理，否则直接删除
        if(callingExpiredTimers_){//防止作为respeat在回调后被reset
            cancelingTimers_.insert(sequence);
        }
    }
    else{
        pendingCancel_.insert(id.sequence());
        return;
    }

}

void TimerQueue::handleRead(){
    uint64_t howmany;
    ::read(timerfd_,&howmany,sizeof(howmany));//清空timerfd的可读事件
    TimePoint now=std::chrono::steady_clock::now();
    auto expire=getExpired(now);//取出所有到期的timer
    callingExpiredTimers_=true;
    cancelingTimers_.clear();

    //执行回调阶段
    for(auto &exp:expire){
        auto it=timersOwned_.find(exp.second);
        if(it==timersOwned_.end()){
            //timersOwnded_里找不到该sequendce，说明已经被删掉，跳过
            continue;
        }
        else{
            auto timer=it->second.get();
            if(!timer->canceled()){
                timer->run();
            }
        }
    }
    callingExpiredTimers_=false;

    //重启/清理阶段
    for(auto& exp:expire){
        if(cancelingTimers_.count(exp.second)){
            //确保被取消不重启，若还在timersOwned_里说明是repeat的，删除掉
            auto it=timersOwned_.find(exp.second);
            if(it!=timersOwned_.end()){
                timersOwned_.erase(it);
            }
        }
        else if(timersOwned_.count(exp.second)){
            //若timer存在
            auto timer=timersOwned_[exp.second].get();
            if(timer->canceled()){
                //执行期间被取消
                timersOwned_.erase(exp.second);
                continue;
            }
            else if(timer->repeat()){//重复timer
                timer->restart(now);
                auto newExp=timer->expiration();
                timers_.emplace(newExp,timer->sequence());
            }
            else{
                timersOwned_.erase(exp.second);
            }
        }
        
    }   
    //重新设置timerfd
    if(!timers_.empty()){//如果还有定时器，重置timerfd为下一个到期时间
        resetTimerfd(timers_.begin()->first);//begin()最早到期
    }
    else{

        resetTimerfd(TimePoint::max());//没有定时器了，设置为无效值
    }
}

std::vector<std::pair<TimePoint,uint64_t>> TimerQueue::getExpired(TimePoint now){
    //只在loop线程
    auto end=timers_.upper_bound({now,UINT64_MAX});
    std::vector<Entry> expire(timers_.begin(),end);
    timers_.erase(timers_.begin(),end);
    return expire;
}

void TimerQueue::resetTimerfd(TimePoint nextExpire){
    auto delta=nextExpire-std::chrono::steady_clock::now();
    if(delta<std::chrono::milliseconds(1)){
        delta=std::chrono::milliseconds(1);//最小1ms，防止过短时间导致系统调用失败
    }
    itimerspec newValue;
    //it_value:第一次触发时间，把delta转为秒和纳秒
    newValue.it_value.tv_sec=std::chrono::duration_cast<std::chrono::seconds>(delta).count();
    newValue.it_value.tv_nsec=std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count()%1000000000;
    //周期触发间隔，此处=0只触发一次
    newValue.it_interval.tv_sec=0;
    newValue.it_interval.tv_nsec=0;
    ::timerfd_settime(timerfd_,0,&newValue,nullptr);
}