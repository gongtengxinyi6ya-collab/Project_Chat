#pragma once
#include <memory>
#include <unordered_map>
#include <set>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <utility>

#include "TimerId.h"
#include "Timer.h"

class EventLoop;
class Channel;
/*管理EventLoop的所有定时器，包括添加，取消，到期触发，重复重排
用timerfd把最近一次到期时间转为epoll可读事件
线程归属：在所属EventLoop线程执行
*/
class TimerQueue{
    using TimePoint=std::chrono::steady_clock::time_point;
    using Duration=std::chrono::milliseconds;
    using TimerCallback=std::function<void()>;
    using Entry=std::pair<TimePoint,Timer*>;//Timer*轻量好排序
    using ActiveTimer=std::pair<Timer*,uint64_t>;//sequence防止地址复用导致误删除
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();
    TimerId addTimer(TimerCallback cb,TimePoint when,Duration interval);//对外API，添加定时器，返回TimerId
    void addTimerInLoop(std::unique_ptr<Timer> timer);//在所属EventLoop线程执行，添加定时器
    void cancel(TimerId timerId);//对外接口，内部转cancelInLoop,取消定时器
    void cancelInLoop(TimerId timerId);//在所属EventLoop线程执行，取消定时器
    void handleRead();//timerfd可读事件回调，执行到期回调，重排下次到期时间
    std::vector<Entry> getExpired(TimePoint now);//批量取出所有到期timer
    void resetTimerfd(TimePoint nextExpire);//重置timerfd的到期时间为nextExpire
private:
    EventLoop* loop_;//所属loop
    int timerfd_;//timerfd_create返回的fd
    std::unique_ptr<Channel> timerfdChannel_;//监听timerfd的读事件
    //排序集合
    std::set<Entry> timers_;//负责按expiration排序，begin()最早到期
    //活跃集合
    std::set<ActiveTimer> activeTimers_;//用于cancel时删除
     
    bool callingExpiredTimers_;//判断是否出土handleRead()的“执行到期回调”阶段
    std::set<ActiveTimer> cancelingTimers_;//执行期被cancel的timer集合
    std::set<ActiveTimer> pendingCancel_;//解决timer还未入队就cancel的竞态
    std::unordered_map<uint64_t,std::unique_ptr<Timer>> timersOwned_;//作为仓库所有权
};