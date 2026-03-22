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

#include "Channel.h"

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
    void runInLoop(std::function<void()> func);//在IO线程中执行任务，如果当前线程就是IO线程则直接执行，否则加入任务队列，唤醒IO线程执行
    void queueInLoop(std::function<void()> func);//将任务加入队列，IO线程会在loop中执行这些任务
    void wakeup();//唤醒IO线程，处理其他线程提交的任务
    void handleWakeup();//处理wakeup事件，执行pendingFunctors_中的任务
    void doPendingFunctors();//执行pendingFunctors_中的任务
private:

    bool looping;
    bool quit_;
    std::vector<struct epoll_event> activeEvents_;//epoll事件列表
    std::unordered_map<int,Channel*> channels_;

    std::vector<std::function<void()>> pendingFunctors_;//存放其他线程提交的任务，IO线程在loop中执行这些任务
    std::mutex mutex_;//保护pendingFunctors_的线程安全
    int wakeupFd_;//唤醒epoll__wait
    std::unique_ptr<Channel> wakeupChannel_;//监听wakeupFd_的Channel
    std::thread::id threadId_;//记录IO线程ID，保证线程安全
};

