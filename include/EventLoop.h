#pragma once

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unordered_map>
#include <sys/epoll.h>
#include <mutex>
#include <sys/eventfd.h>
#include <thread>
#include <functional>
#include <memory>
#include "Channel.h"
#include "TimerQueue.h"
#include "TimerId.h"
#include "logger/LogMacros.h"

const int EPOLL_MAX_EVENTS=100;


//负责事件循环，调用select/epoll,分发事件给Channel
class EventLoop{
 public:
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void addChannel(Channel* channel);
    void updateChannel(Channel* channel);
    void removeChannel(int fd);

    bool isInLoopThread() const;//判断是否在IO线程中
    template<class F>
    void runInLoop(F&& func);//在IO线程中执行任务，如果当前线程就是IO线程则直接执行，否则加入任务队列，唤醒IO线程执行
    template<class F>
    void queueInLoop(F&& func);//将任务加入队列，IO线程会在loop中执行这些任务
    void wakeup();//唤醒IO线程，处理其他线程提交的任务
    void handleWakeup();//处理wakeup事件，执行pendingFunctors_中的任务
    void doPendingFunctors();//执行pendingFunctors_中的任务

    //定时器接口
    TimerId runAt(TimePoint when,TimerCallback cb);//在绝对时间点when执行一次回调
    TimerId runAfter(Duration delay,TimerCallback cb);//延迟delay后执行一次回调
    TimerId runEvery(Duration interval,TimerCallback cb);//每隔interval重复执行回调
    void cancel(TimerId id);//取消一个定时器
private:
    //任务基类
class TaskBase{
public:
    virtual ~TaskBase()=default;
    virtual void call()=0;
};
template<class F>
class TaskModel:public TaskBase{
public:
    F f;
    explicit TaskModel(F&& f): f(std::forward<F>(f)){}
    void call() override{f();};
};

    bool looping;
    bool quit_;
    std::vector<struct epoll_event> activeEvents_;//epoll事件列表
    std::unordered_map<int,Channel*> channels_;
    int epollfd_;//epoll文件描述符
    

    std::vector<std::unique_ptr<TaskBase>> pendingFunctors_;//存放其他线程提交的任务，IO线程在loop中执行这些任务
    std::mutex mutex_;//保护pendingFunctors_的线程安全
    int wakeupFd_;//唤醒epoll__wait
    std::unique_ptr<Channel> wakeupChannel_;//监听wakeupFd_的Channel
    std::thread::id threadId_;//记录IO线程ID，保证线程安全

    //定时器属性
    std::unique_ptr<TimerQueue> timerQueue_;//每个loop一份定时器管理器

};


//
template<class F>
void EventLoop::runInLoop(F&& func){
    if(isInLoopThread()){
        func();
    }
    else{
        queueInLoop(std::forward<F>(func));
        wakeup();
    }
}

template<class F>
void EventLoop::queueInLoop(F&& func){
    {
        std::lock_guard lk(mutex_);
        pendingFunctors_.push_back(std::make_unique<TaskModel<F>>(std::forward<F>(func)));
    }
    if(!isInLoopThread()){
        wakeup();
    }
}
