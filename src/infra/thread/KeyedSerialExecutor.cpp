#include "infra/thread/ThreadPool.h"
#include "infra/thread/KeyedSerialExecutor.h"

namespace infra::thread{
KeyedSerialExecutor::KeyedSerialExecutor(std::size_t shardCount,std::size_t queueCapacityPerShard)
{
    for(size_t i=0;i<shardCount;i++){
        shards_.emplace_back(std::make_unique<ThreadPool>(1,queueCapacityPerShard));
    }
}

TaskSubmitResult KeyedSerialExecutor::submit(std::string_view key,Task task){
    if(!accepting_.load(std::memory_order_acquire)){
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
    for(const auto& pool:shards_){
        pool->stop();
    }
}
}