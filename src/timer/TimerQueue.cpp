#include <sys/timerfd.h>
#include <unistd.h>
#include "timer/TimerQueue.h"
#include "timer/Timer.h"
#include "EventLoop.h"
#include "Channel.h"
#include "logger/LogMacros.h"
#include <system_error>
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
    loop_->assertInLoopThread();
    timerfdChannel_->disableAll();
    if(timerfdChannel_->inEpoll()){
        timerfdChannel_->remove();
    }
    timerfdChannel_.reset();
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
        {
            std::lock_guard lk(pendingMutex_);
            pendingAddSequences_.insert(sequence);
        }
        loop_->runInLoop([this,timer=std::move(timer)]()mutable{
            addTimerInLoop(std::move(timer));
        });
    }
    return id;
}

void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> timer){
    {
        std::lock_guard lk(pendingMutex_);
        pendingAddSequences_.erase(timer->sequence());
    }

    if(pendingCancel_.contains(timer->sequence())){
        //如果这个timer还未入队就被cancel了，移除
        pendingCancel_.erase(timer->sequence());
        return;
    }
    //判断是否早于当前最早到期
    bool earliestChanged=timers_.empty()||timer->expiration()<timers_.begin()->first;
    auto sequence=timer->sequence();
    auto expiration=timer->expiration();
    timers_.insert({expiration,sequence});
    timersOwned_.emplace(sequence,std::move(timer));
    if(earliestChanged){
        resetTimerfd(expiration);
    }
}
void TimerQueue::cancel(TimerId id){
    if(!id.valid()||!(id.ownerLoop()==loop_)){
        return;
    }
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
    //查找timer
    auto it=timersOwned_.find(id.sequence());
    if(it!=timersOwned_.end()){//找到timer
        auto expiration=it->second->expiration();
        auto sequence=it->second->sequence();
        if(callingExpiredTimers_&&activeExpiredTimers_.contains(sequence)){
            //属于本轮到期集合
            it->second->cancel();
            cancelingTimers_.insert(sequence);
        }
        else{//不是本轮到期timer
            bool wasEarliest=sequence==timers_.begin()->second?true:false;
            timers_.erase({expiration,sequence});//删除排序项
            timersOwned_.erase(it);//删除所有权
            if(wasEarliest){//删除的是最早timer
                if(timers_.empty()){
                    disarmTimerfd();
                }
                else{
                    resetTimerfd(timers_.begin()->first);
                }
            }
        }
        
    }
    else{//未找到timer
        {
            std::lock_guard lk(pendingMutex_);
            if(!pendingAddSequences_.contains(id.sequence())){
                return ;//不存在直接忽略
            }
        }
        pendingCancel_.insert(id.sequence());//timer等待添加
    }

}

void TimerQueue::handleRead(){
    //timerfd读取
    uint64_t expirationCount{0};
    while(true){
        const ssize_t n=::read(timerfd_,&expirationCount,sizeof(expirationCount));//清空timerfd的可读事件
        if(n==static_cast<ssize_t>(sizeof(expirationCount))){//成功读取8字节
            break;
        }
        if(n<0){//错误处理
            const int savedErrno = errno;
            if(savedErrno==EINTR){//重试
                continue;
            }
            else if(savedErrno==EAGAIN){
                return;
            }
            else{
                const std::error_code ec(savedErrno, std::system_category());
                LOG_ERROR("Failed to read timerfd,errno: "+ec.message());
                return;
            }
        }
        LOG_ERROR("Unexpected timerfd read size");
        return;
    }

    TimePoint now=std::chrono::steady_clock::now();
    auto expire=getExpired(now);//取出所有到期的timer
    for(const auto& exp:expire){
        activeExpiredTimers_.insert(exp.second);
    }
    callingExpiredTimers_=true;
    cancelingTimers_.clear();
    {
        struct Guard{
            bool& callingExpiredTimers;
            ~Guard(){
                callingExpiredTimers=false;
            }
        }guard{callingExpiredTimers_};
        //执行回调阶段
        for(auto &exp:expire){
            auto it=timersOwned_.find(exp.second);
            if(it==timersOwned_.end()){
                //timersOwnded_里找不到该sequendce，说明已经被删掉，跳过
                continue;
            }
            else{
                auto timer=it->second.get();
                if(timer->canceled()){
                    continue;
                }
                try{
                    timer->run();
                }catch (const std::exception& e) {
                    LOG_ERROR(
                        "Timer callback threw exception, sequence="+std::to_string(timer->sequence())+std::string(e.what()));
                } catch (...) {
                    LOG_ERROR( "Timer callback threw unknown exception, sequence="+ std::to_string(timer->sequence()));
                }
            }
        }
    }

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
        disarmTimerfd();//没有定时器
    }
    //清空
    activeExpiredTimers_.clear();
    cancelingTimers_.clear();
}

std::vector<std::pair<TimePoint,uint64_t>> TimerQueue::getExpired(TimePoint now){
    //只在loop线程
    auto end=timers_.upper_bound({now,UINT64_MAX});
    std::vector<Entry> expire(timers_.begin(),end);
    timers_.erase(timers_.begin(),end);
    return expire;
}

bool TimerQueue::resetTimerfd(TimePoint nextExpire)noexcept{
    auto delta=nextExpire-std::chrono::steady_clock::now();//计算与当前的差
    if(delta<std::chrono::milliseconds(1)){
        delta=std::chrono::milliseconds(1);//最小1ms，防止过短时间导致系统调用失败
    }
    itimerspec newValue{};
    //it_value:第一次触发时间，把delta转为秒和纳秒
    newValue.it_value.tv_sec=std::chrono::duration_cast<std::chrono::seconds>(delta).count();
    newValue.it_value.tv_nsec=std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count()%1000000000;
    //周期触发间隔，此处=0只触发一次
    newValue.it_interval.tv_sec=0;
    newValue.it_interval.tv_nsec=0;
    if(::timerfd_settime(timerfd_,0,&newValue,nullptr)==-1){
        const int savedErrno=errno;
        const std::error_code ec(savedErrno, std::system_category());
        LOG_ERROR("timerfd_settime failed: "+ec.message());
        return false;
    }
    return true;
}

bool TimerQueue::disarmTimerfd() noexcept{
    itimerspec newValue{};
    if(::timerfd_settime(timerfd_,0,&newValue,nullptr)==-1){
        const int savedErrno=errno;
        LOG_ERROR("timerfd_settime failed: "+std::string(std::strerror(savedErrno)));
        return false;
    }
    return true;
}