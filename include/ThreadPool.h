#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include "threadSaveQueue.h"

class JoinThreads{
public:
    explicit JoinThreads(std::vector<std::thread>& threads);
    ~JoinThreads();

private:
    std::vector<std::thread>& threads_;
};

//线程池类，管理工作线程和任务队列

class ThreadPool{
public:
    using Task=std::function<void()>;
    ThreadPool();
    ~ThreadPool();
    template<typename FunctionType>
    void submit(FunctionType func);

private:
    ThreadSafeQueue<Task> taskQueue_;//任务队列
    std::atomic_bool stop_;//停止标志
    std::vector<std::thread> threads_;//工作线程
    JoinThreads joiner_;//线程管理类，析构时自动join所有线程

    void workerThread();//工作线程函数，从任务队列中取任务并执行
};