#pragma once
#include <string>
#include <cstdint>
#include <atomic>
#include "storage/RepositoryBundle.h"
#include "config/MaintenanceConfig.h"
#include "infra/maintenance/MaintenanceStats.h"
/*统一执行后台清理任务*/

namespace infra::maintenance{
class MaintenanceService {
public:
    MaintenanceService(MaintenanceConfig config, storage::RepositoryBundle repos);

    MaintenanceStats runOnce();

private:
    MaintenanceConfig config_;
    storage::RepositoryBundle repos_;

    std::atomic_bool running_{false};//正在执行
    std::atomic<int64_t> lastRunAtMs_{0};//上次执行时间
    std::atomic<int64_t> lastSuccessAtMs_{0};//上次成功执行时间

    int64_t nowMs() const;

    size_t cleanupExpiredSessions(int64_t nowMs);//清理过期session
    size_t cleanupRevokedSessions(int64_t nowMs);//清理注销session
    size_t cleanupHandledFriendRequests(int64_t nowMs);//清理已经处理的好友申请
    size_t cleanupHandledGroupJoinRequests(int64_t nowMs);//清理已经审批的入群申请
    size_t cleanupOfflineIndexes(int64_t nowMs);//清理离线索引
};
}