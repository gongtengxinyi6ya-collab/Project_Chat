#pragma once
#include <cstdint>
#include "third_party/json.hpp"

/*独立配置消息持久化线程池*/
class MessageAsyncConfig{
public:
    static MessageAsyncConfig fromJson(const nlohmann::json& json);
    void applyEnvOverrides();
    void validateOrThrow() const;

    bool enabled{true};//是否启用消息异步持久化
    std::size_t workerThreads{1};//消息持久化工作线程数
    std::size_t queueCapacity{256};//有界任务队列容量
    std::uint32_t queueWarnPercent{80};//健康检查中判定队列拥塞的比例
};