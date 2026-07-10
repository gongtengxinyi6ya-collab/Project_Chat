#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include "infra/thread/ThreadSafeQueue.h"
#include "infra/thread/TheadType.h"
/*线程池负责：
创建工作线程
调用队列阻塞等待
执行任务
捕获异常
停止接收任务
选择Drain/Discard
等待线程退出，统计运行状态*/
namespace infra::thread{

class ThreadPool{
public:
    using Task=std::function<void()>;
    explicit ThreadPool(size_t threadCount,size_t queueCapacity);
    ~ThreadPool();
   
    TaskSubmitResult submit(Task task);
    void stop(ThreadPoolStopMode mode = ThreadPoolStopMode::Drain);
    ThreadPoolStats stats() const;

    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;

    ThreadPool(ThreadPool&&)=delete;
    ThreadPool& operator=(ThreadPool&&)=delete;
private:
    ThreadSafeQueue<Task> taskQueue_;//任务队列
    std::vector<std::thread> threads_;//工作线程

    std::atomic<ThreadPoolState> state_{ThreadPoolState::Stopped};
    std::atomic<size_t> activeTasks_{0};//正在执行任务数量

    std::atomic<uint64_t> submittedTasks_{0};
    std::atomic<uint64_t> completedTasks_{0};
    std::atomic<uint64_t> failedTasks_{0};
    std::atomic<uint64_t> rejectedFull_{0};
    std::atomic<uint64_t> rejectedStopped_{0};

    mutable std::mutex stopMutex_;
    std::condition_variable stoppedCv_;
    

    void workerThread();//工作线程函数，从任务队列中取任务并执行
};
}