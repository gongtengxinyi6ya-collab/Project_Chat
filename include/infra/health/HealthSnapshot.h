#pragma once
#include <string>
#include <cstdint>
#include "storage/sql/SqlPoolStats.h"

/*表示服务当前健康状态，作为HealthService的输出结果*/

namespace infra::health {

enum class HealthStatus {//总体状态
    Healthy,//核心依赖正常
    Degraded,//部分依赖不可用，但服务可继续运行
    Unhealthy//核心依赖不可用
};

struct HealthSnapshot {
    HealthStatus status{HealthStatus::Healthy};

    bool sqlEnabled{false};//是否使用SQL存储
    bool sqlHealthy{true};//SQL连接池是否健康
    storage::SqlConnectionPoolStats sqlStats{};//连接池状态

    bool redisEnabled{false};//是否启用Redis
    bool redisHealthy{true};//Redis连接是否健康

    size_t onlineConnections{0};//当前在线TCP连接数
    uint64_t uptimeMs{0};//服务启动后的运行时间

    uint64_t logDropped{0};//异步日志丢弃数量
    uint64_t logWritten{0};//异步日志已写数量

    std::string reason{};//健康状态降级原因
};

}