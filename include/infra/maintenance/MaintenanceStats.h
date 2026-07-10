#pragma once
#include <string>
#include <cstdint>
/*表示一轮维护任务的执行结果*/
namespace infra::maintenance{
struct MaintenanceStats {
    bool ok{true};//是否成功
    std::string error{};//错误记录摘要

    size_t expiredSessionsDeleted{0};//删除的过期session数量
    size_t revokedSessionsDeleted{0};//删除的已注销session数量
    size_t friendRequestsDeleted{0};//删掉的已处理好友申请数量
    size_t groupJoinRequestsDeleted{0};//删除的已处理入群申请数量
    size_t offlineIndexesDeleted{0};//删除的过期离线索引数

    int64_t startedAtMs{0};//本轮执行开始时间
    int64_t finishedAtMs{0};//本轮执行结束时间

    size_t totalDeleted() const{return expiredSessionsDeleted+revokedSessionsDeleted+
    friendRequestsDeleted+groupJoinRequestsDeleted+offlineIndexesDeleted;}//总删除数量之和
};

struct MaintenanceSnapshot {//维护服务从启动到当前的累计运行状态
    bool running{false};
    bool hasRun{false};
    bool lastRunOk{true};

    uint64_t totalRuns{0};
    uint64_t successRuns{0};
    uint64_t failedRuns{0};
    uint64_t skippedRuns{0};

    int64_t lastRunAtMs{0};
    int64_t lastSuccessAtMs{0};
    uint64_t lastDurationMs{0};
    size_t lastDeleted{0};

    std::string lastError{};
};
}