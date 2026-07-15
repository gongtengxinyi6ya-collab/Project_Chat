#pragma once
#include <functional>
#include <string_view>
#include <atomic>
#include <vector>
#include <memory>
#include <stddef.h>

#include "infra/thread/ThreadTypes.h"
/*相同key的任务加入同一个单线程分片，不同key可以并行*/
namespace infra::thread{
class ThreadPool;

class KeyedSerialExecutor{
public:
    using Task = std::function<void()>;
    KeyedSerialExecutor(std::size_t shardCount,std::size_t queueCapacityPerShard);//创建多个分片线程池
    TaskSubmitResult submit(std::string_view key,Task task);//提交任务
    std::vector<ThreadPoolStats> stats() const;//获取所有shard状态
    void stop(ThreadPoolStopMode mode);
    ThreadPoolStats aggregateStats() const;//状态汇总
private:
    std::vector<std::unique_ptr<ThreadPool>> shards_;//保存多个线程池分片
    std::hash<std::string_view> hasher_;//根据key找shards
    std::atomic<bool> accepting_{true};//是否接受新任务
};
}