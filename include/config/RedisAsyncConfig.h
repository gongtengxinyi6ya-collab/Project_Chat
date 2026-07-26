#pragma once

#include <cstddef>
#include <cstdint>

#include "third_party/json.hpp"

/*Redis执行器配置*/
class RedisAsyncConfig {
public:
    static RedisAsyncConfig fromJson(const nlohmann::json& json);
    void applyEnvOverrides();
    void validateOrThrow() const;

    bool enabled{true};//是否启用redis异步执行器
    std::size_t workerThreads{4};//执行器分片数
    std::size_t queueCapacityPerShard{512};//每个分片的有界队列容量
    std::uint32_t queueWarnPercent{80};//健康检查中
    bool failOpen{true};//redis异常时是否放行执行
};