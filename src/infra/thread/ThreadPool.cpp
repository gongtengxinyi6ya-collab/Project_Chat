#include "infra/thread/ThreadPool.h"
#include <exception>
#include <stdexcept>
#include "logger/LogMacros.h"
namespace infra::thread{

ThreadPool::ThreadPool(size_t threadCount,size_t queueCapacity)
:taskQueue_(ThreadSafeQueue<Task>(queueCapacity))
{
    if(threadCount<=0){
        throw std::invalid_argument("threadCount invalid");
    }
    for(size_t i=0;i<threadCount;i++){//循环创建工作线程
        try{
            threads_.emplace_back(&ThreadPool::workerThread,this);
        }catch(const std::exception& e){
            stop(ThreadPoolStopMode::Discard);
            throw std::runtime_error(e.what());
        }
    }
    state_.store(ThreadPoolState::Running,std::memory_order_release);
}

ThreadPool::~ThreadPool(){
    stop(ThreadPoolStopMode::Discard);
}


TaskSubmitResult ThreadPool::submit(Task task){
    if(!task){//任务为空
        return TaskSubmitResult::InvalidTask;
    }
    if(state_.load(std::memory_order_relaxed)!=ThreadPoolState::Running){
        return TaskSubmitResult::Stopping;
    }
    auto result=taskQueue_.push(std::move(task));
    auto submitResult=taskResultFromqueuePushResult(result);
    //更新对应统计
    if(submitResult==TaskSubmitResult::Accepted){
        submittedTasks_.fetch_add(1,std::memory_order_relaxed);
    }
    else if(submitResult==TaskSubmitResult::QueueFull){
        rejectedFull_.fetch_add(1,std::memory_order_relaxed);
    }
    else if(submitResult==TaskSubmitResult::Stopping){
        rejectedFull_.fetch_add(1,std::memory_order_relaxed);
    }
    return submitResult;
   
}

void ThreadPool::workerThread(){
    Task task;
    while(taskQueue_.waitPop(task)){
        activeTasks_.fetch_add(1,std::memory_order_relaxed);
        try{
            task();
            activeTasks_.fetch_sub(1,std::memory_order_relaxed);
            completedTasks_.fetch_add(1,std::memory_order_relaxed);
        }catch(const std::exception& e){
            failedTasks_.fetch_add(1,std::memory_order_relaxed);
            LOG_ERROR("Failed to complete the task:"+std::string(e.what()));
        }catch(...){
            failedTasks_.fetch_add(1,std::memory_order_relaxed);
            LOG_ERROR("Failed to complete the task");
        }
    }
}

void ThreadPool::stop(ThreadPoolStopMode mode){
    if(state_.load(std::memory_order_acquire)==ThreadPoolState::Stopped){
        return;
    }
    std::unique_lock lk(stopMutex_);
    stoppedCv_.wait(lk,[this]{
        return state_.load(std::memory_order_acquire)==ThreadPoolState::Stopping;
    });
    state_.store(ThreadPoolState::Stopping,std::memory_order_release);
    if(mode==ThreadPoolStopMode::Drain){
        taskQueue_.close(QueueCloseMode::Drain);
    }
    else{
        taskQueue_.close(QueueCloseMode::Discard);
    }
    for(auto& thread:threads_){
        if(thread.joinable()){
            thread.join();
        }
    }
    state_.store(ThreadPoolState::Stopped,std::memory_order_release);
    stoppedCv_.notify_one();
    return ;

}

ThreadPoolStats ThreadPool::stats()const{
    return ThreadPoolStats{.state=state_.load(std::memory_order_relaxed),
        .workerCount=threads_.size(),.queuedTasks=taskQueue_.size(),
        .activeTasks=activeTasks_.load(std::memory_order_relaxed),
        .submittedTasks=submittedTasks_.load(std::memory_order_relaxed),
        .completedTasks=completedTasks_.load(std::memory_order_relaxed),
        .failedTasks=failedTasks_.load(std::memory_order_relaxed),
        .rejectedFull=rejectedFull_.load(std::memory_order_relaxed),
        .rejectedStopped=rejectedStopped_.load(std::memory_order_relaxed)
    };
}
}