#pragma once
#include <cstdint>
#include <cstddef>
namespace infra::thread{
enum class QueuePushResult : uint8_t {//入队结果
    Ok,//入队成功
    Full,//达到容量上限
    Closed//队列关闭
};

enum class QueueCloseMode : uint8_t {
    Drain,//关闭后保留已有元素，允许消费者取完
    Discard//关闭后直接清空尚未消费的元素
};

enum class ThreadPoolState : uint8_t {//线程池状态
    Running,
    Stopping,
    Stopped
};

enum class ThreadPoolStopMode : uint8_t {//线程池停止模式
    Drain,
    Discard
};

enum class TaskSubmitResult : uint8_t {//任务提交结果
    Accepted,
    QueueFull,
    Stopping,
    InvalidTask
};
struct ThreadPoolStats {//状态统计
    ThreadPoolState state{ThreadPoolState::Stopped};

    size_t workerCount{0};
    size_t queuedTasks{0};
    std::size_t queueCapacity{0};
    size_t activeTasks{0};

    uint64_t submittedTasks{0};
    uint64_t completedTasks{0};
    uint64_t failedTasks{0};
    uint64_t rejectedFull{0};
    uint64_t rejectedStopped{0};
};
inline TaskSubmitResult taskResultFromqueuePushResult(QueuePushResult value){
    switch(value){
        case QueuePushResult::Ok:
            return TaskSubmitResult::Accepted;
        case QueuePushResult::Full:
            return TaskSubmitResult::QueueFull;
        case QueuePushResult::Closed:
            return TaskSubmitResult::Stopping;
        default :
            return TaskSubmitResult::Stopping;
    }
} 
}