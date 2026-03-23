#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
class EventLoop;
//保证一个线程只跑一个EventLoop，提供创建和获取EventLoop的接口
class EventLoopThread{
public:
    EventLoopThread(const std::string& name="EventLoopThread");
    ~EventLoopThread();

    EventLoop* startLoop();//启动线程，创建EventLoop对象，并返回指向该对象的指针

private:
    void threadFunc();//线程函数，创建EventLoop对象，执行事件循环


    std::thread thread_;//工作线程本体
    EventLoop* loop_;//线程内的EventLoop对象指针
    std::mutex mutex_;//保护loop_的线程安全
    std::condition_variable cond_;//条件变量，等待loop_创建完成
    bool exiting_;//标志线程退出
    std::string name_;//线程名称，调试用

};