#include "timer/Timer.h"

Timer::Timer(TimerCallback cb,TimePoint when,Duration interval)
:callback_(std::move(cb)),expiration_(when),interval_(interval),repeat_(interval_.count()>0),canceled_(false)
{
    static std::atomic<uint64_t> s_numCreated{0};
    sequence_=++s_numCreated;

}
void Timer::run() const{
    if(callback_){
        callback_();
    }
}
void Timer::restart(TimePoint now){//对重复定时器，计算下一次到期时间
    if(repeat_)
        expiration_=now+interval_;
    else
        expiration_=TimePoint::min();//设置为无效值
}

void Timer::cancel(){
    canceled_=true;
}