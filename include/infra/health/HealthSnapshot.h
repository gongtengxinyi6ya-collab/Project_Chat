#pragma once
#include <string>
#include <cstdint>
#include <cstddef>
#include "storage/sql/SqlPoolStats.h"
#include "logger/LoggerStats.h"
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
    bool sqlAcquireTimeoutIncreased{false};//本轮检查相比上一轮是否增加SQL获取理解超时
    storage::SqlConnectionPoolStats sqlStats{};//连接池状态

    bool redisEnabled{false};//是否启用Redis
    bool redisHealthy{true};//Redis连接是否健康
    bool redisPingChecked{false};//表示Redis状态由ping得到

    size_t onlineConnections{0};//当前在线TCP连接数
    uint64_t uptimeMs{0};//服务启动后的运行时间

    LoggerStats loggerStats{};//日志状态
    std::string reason{};//健康状态降级原因
};

}