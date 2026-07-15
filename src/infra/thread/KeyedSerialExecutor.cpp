#include "infra/thread/ThreadPool.h"
#include "infra/thread/KeyedSerialExecutor.h"
#include <exception>
namespace infra::thread{
KeyedSerialExecutor::KeyedSerialExecutor(std::size_t shardCount,std::size_t queueCapacityPerShard)
{   
    if(shardCount<0||queueCapacityPerShard<0){
        throw std::invalid_argument("KeyedSerialExecutor:shardCount or queueCapacityPerShard invalid");
    }
    for(size_t i=0;i<shardCount;i++){
        shards_.emplace_back(std::make_unique<ThreadPool>(1,queueCapacityPerShard));
    }
}

TaskSubmitResult KeyedSerialExecutor::submit(std::string_view key,Task task){
    if(!task){
        return TaskSubmitResult::InvalidTask;
    }
    if(!accepting_.load(std::memory_order_acquire)){
        return TaskSubmitResult::Stopping;
    }
    if(shards_.empty()){
        return TaskSubmitResult::Stopping;
    }
    auto index=hasher_(key)%shards_.size();
    return shards_[index]->submit(std::move(task));

}
std::vector<ThreadPoolStats> KeyedSerialExecutor::stats() const{
    std::vector<ThreadPoolStats> stats;
    for(const auto& pool:shards_){
        stats.emplace_back(pool->stats());
    }
    return stats;
}

void KeyedSerialExecutor::stop(ThreadPoolStopMode mode){
    if(!accepting_.load(std::memory_order_acquire)){
        return;
    }
    for(const auto& pool:shards_){
        pool->stop(mode);
    }
}

ThreadPoolStats KeyedSerialExecutor::aggregateStats() const{
    ThreadPoolStats total;
    bool isruning=true;
    bool isStopping=false;
    bool hasStopped=true;
    for(const auto& shard:shards_){
        auto stat=shard->stats();
        total.workerCount+=stat.workerCount;
        total.queuedTasks+=stat.queuedTasks;
        total.queueCapacity+=stat.queueCapacity;
        total.activeTasks+=stat.activeTasks;
        total.submittedTasks+=stat.submittedTasks;
        total.completedTasks+=stat.completedTasks;
        total.rejectedFull+=stat.rejectedFull;
        total.rejectedStopped+=stat.rejectedStopped;
        if(stat.state!=ThreadPoolState::Running){
            isruning=false;
        }
        else if(stat.state==ThreadPoolState::Stopping){
            isStopping=true;
        }
        else if(stat.state!=ThreadPoolState::Stopped){
            hasStopped=false;
        }
    }
    if(isruning){
        total.state=ThreadPoolState::Running;
    }
    else if(isStopping){
        total.state=ThreadPoolState::Stopping;
    }
    else{
        total.state=ThreadPoolState::Stopped;
    }
    return total;
}
}