#include "infra/thread/ThreadPool.h"
#include <exception>
#include <stdexcept>

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
    auto sumitResult=taskResultFromqueuePushResult(result);
    //更新对应统计
    
}

void ThreadPool::workerThread(){
    
}
}