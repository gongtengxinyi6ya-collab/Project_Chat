#pragma once

#include <atomic>
#include <cstdint>

namespace infra::health {

struct RedisHealthProbeSnapshot {//redis健康探测快照
    bool hasResult{false};
    bool healthy{true};
    bool inFlight{false};

    std::int64_t lastCheckAtMs{0};
    std::int64_t lastLatencyUs{0};
    std::uint64_t failedChecks{0};
};

/*redis健康检测；保存最近一次异步redis ping结果*/
class RedisHealthProbe {
public:
    bool tryBegin() noexcept;//尝试标记一次redis健康检查正在进行
    void complete(bool healthy,std::int64_t checkedAtMs,std::int64_t latencyUs) noexcept;//结束本次健康检查，并保存检查结果
    void cancel() noexcept;//取消当前检查占用状态
    RedisHealthProbeSnapshot snapshot()const noexcept;//原子读取当前redis健康状态

private:
    std::atomic<bool> inFlight_{false};//当前是否已经有一次redis健康检查正在执行
    std::atomic<bool> hasResult_{false};//是否至少完成过一次有效健康检查
    std::atomic<bool> healthy_{true};//最近一次已完成的健康检查是否成功

    std::atomic<std::int64_t> lastCheckAtMs_{0};//最近一次完成健康检查的时间
    std::atomic<std::int64_t> lastLatencyUs_{0};//最近一次redis健康检查耗时
    std::atomic<std::uint64_t> failedChecks_{0};//服务启动以来累计失败的redis健康检查次数
};

}