#include "infra/health/RedisHealthProbe.h"

namespace infra::health{
    bool RedisHealthProbe::tryBegin() noexcept{
        bool expected=false;
        return inFlight_.compare_exchange_strong(expected,true,std::memory_order_acq_rel,std::memory_order_acquire);
    }
    void RedisHealthProbe::complete(bool healthy,std::int64_t checkedAtMs,std::int64_t latencyUs) noexcept{
        //保存成功状态、延迟、检查时间
        healthy_.store(healthy,std::memory_order_relaxed);
        lastCheckAtMs_.store(checkedAtMs,std::memory_order_relaxed);
        lastLatencyUs_.store(latencyUs,std::memory_order_relaxed);
        if(!healthy_.load(std::memory_order_relaxed)){//失败增加失败次数
            failedChecks_.fetch_add(1,std::memory_order_relaxed);
        }
        hasResult_.store(true,std::memory_order_release);
        inFlight_.store(false,std::memory_order_release);
    }
    void RedisHealthProbe::cancel() noexcept{
        inFlight_.store(false,std::memory_order_release);
    }
    RedisHealthProbeSnapshot RedisHealthProbe::snapshot()const noexcept{
        return {
            .hasResult=hasResult_.load(std::memory_order_acquire),
            .healthy=healthy_.load(std::memory_order_relaxed),
            .inFlight=inFlight_.load(std::memory_order_acquire),
            .lastCheckAtMs=lastCheckAtMs_.load(std::memory_order_relaxed),
            .lastLatencyUs=lastLatencyUs_.load(std::memory_order_relaxed),
            .failedChecks=failedChecks_.load(std::memory_order_relaxed)
        };
    }

}