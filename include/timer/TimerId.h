#pragma once
#include <cstdint>
#include <memory>
class EventLoop;

//取消句柄，一个TimerId对于一个Timer身份，负责定位与取消
class TimerId{
public:
    TimerId();//默认无效id
    explicit TimerId(uint64_t seq,EventLoop* loop);//创建有效id

    bool valid() const{return sequence_!=0;};//判断是否为有效定时器句柄
    uint64_t sequence()const {return sequence_;};
    EventLoop* ownerLoop()const noexcept{return loop_;}//用于cancel时检查Timer是否属于当前evetLoop

private:
    uint64_t sequence_;//唯一标识一个Timer
    EventLoop* loop_;//标识timer属于哪个EventLoop，
};