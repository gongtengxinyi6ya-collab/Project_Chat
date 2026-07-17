#include "infra/health/HealthFormatter.h"
#include "storage/sql/SqlPoolStats.h"
#include <sstream>
namespace infra::health{
    std::string formatHealthSnapshot(const HealthSnapshot& snapshot){
    std::ostringstream oss;

    oss << "status=" << healthStatusToString(snapshot.status)
        << ", sqlEnabled=" << snapshot.sqlEnabled
        << ", sqlHealthy=" << snapshot.sqlHealthy
        <<", SqlStats: "<<storage::formatSqlPoolStats(snapshot.sqlStats)
        << ", messageSqlEnabled="<< snapshot.messageSqlEnabled<< ", messageSqlHealthy="<< snapshot.messageSqlHealthy<< ", messageSqlStats="<< storage::formatSqlPoolStats(snapshot.messageSqlStats)
        << ", redisEnabled=" << snapshot.redisEnabled
        << ", redisHealthy=" << snapshot.redisHealthy
        <<", redisPingChecked="<<snapshot.redisPingChecked
        << ", onlineConnections=" << snapshot.onlineConnections
        << ", uptimeMs=" << snapshot.uptimeMs
        <<", logAsyncEnabled="<<snapshot.loggerStats.asyncEnabled
        <<", logAsyncRunning="<<snapshot.loggerStats.asyncRunning
        <<", logWritten="<<snapshot.loggerStats.written
        <<", logDropped="<<snapshot.loggerStats.dropped
        <<", logQueueSize="<<snapshot.loggerStats.queueSize
        <<", maintenanceEnabled="<<snapshot.maintenanceEnabled;

    if (!snapshot.reason.empty()) {
        oss << ", reason=" << snapshot.reason;
    }
    if(snapshot.maintenanceEnabled){
        oss
        <<", maintenanceHealthy="<<snapshot.maintenanceHealthy
        <<", maintenanceRunning="<<snapshot.maintenance.running
        <<", maintenanceHasRun="<<snapshot.maintenance.hasRun
        <<", maintenanceLastRunOk="<<snapshot.maintenance.lastRunOk
        <<", maintenanceLastRunAtMs="<<snapshot.maintenance.lastRunAtMs
        <<", maintenanceLastSuccessAtMs="<<snapshot.maintenance.lastSuccessAtMs
        <<", maintenanceLastDurationMs="<<snapshot.maintenance.lastDurationMs
        <<", maintenanceLastDeleted="<<snapshot.maintenance.lastDeleted
        <<", maintenanceTotalRuns="<<snapshot.maintenance.totalRuns
        <<", maintenanceSuccessRuns="<<snapshot.maintenance.successRuns
        <<", maintenanceFailedRuns="<<snapshot.maintenance.failedRuns
        <<", maintenanceSkippedRuns="<<snapshot.maintenance.skippedRuns
        <<", maintenanceStale="<<snapshot.maintenanceStale
        <<", maintenanceRunningTooLong="<<snapshot.maintenanceRunningTooLong
        <<", lastError="<<snapshot.maintenance.lastError;
    }
    oss << ", messageExecutorEnabled="
    << snapshot.messageExecutorEnabled
    << ", messageExecutorHealthy="
    << snapshot.messageExecutorHealthy
    << ", messageExecutorSaturated="
    << snapshot.messageExecutorSaturated
    << ", messageWorkers="
    << snapshot.messageExecutorStats.workerCount
    << ", messageActive="
    << snapshot.messageExecutorStats.activeTasks
    << ", messageQueueSize="
    << snapshot.messageExecutorStats.queuedTasks
    << ", messageQueueCapacity="
    << snapshot.messageExecutorStats.queueCapacity
    << ", messageSubmitted="
    << snapshot.messageExecutorStats.submittedTasks
    << ", messageCompleted="
    << snapshot.messageExecutorStats.completedTasks
    << ", messageFailed="
    << snapshot.messageExecutorStats.failedTasks
    << ", messageRejectedFull="
    << snapshot.messageExecutorStats.rejectedFull
    << ", messageRejectedStopped="
    << snapshot.messageExecutorStats.rejectedStopped;
    return oss.str();
}
}