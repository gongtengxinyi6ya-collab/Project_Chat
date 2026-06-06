#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <cstdint>
#include <cassert>
#include <atomic>
//负责描述什么时候执行什么，是否重复，如何计算下一次到期
using TimePoint=std::chrono::steady_clock::time_point;
using Duration=std::chrono::milliseconds;
class Timer{
    using TimerCallback=std::function<void()>;
public:
    Timer(TimerCallback cb,TimePoint when,Duration interval);//传入回调函数，到期时间点，重复间隔
    void run() const;//执行回调函数
    TimePoint expiration() const { return expiration_; }//获取到期时间点
    bool repeat() const { return repeat_; }//是否重复
    uint64_t sequence() const { return sequence_; }//获取全局唯一id
    void restart(TimePoint now);//重启，计算下一次到期时间点
    void cancel();//设置canceled_=true;
    bool canceled() const{return canceled_;};//读取标记


private:
    TimerCallback callback_;//到期执行的函数
    TimePoint expiration_;//下次到期时间点
    Duration interval_;//重复间隔
    bool repeat_;//是否重复
    uint64_t sequence_;//全局唯一id
    bool canceled_;//是否取消
    
};