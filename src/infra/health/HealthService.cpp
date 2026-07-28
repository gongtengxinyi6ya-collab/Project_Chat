#include "infra/health/HealthService.h"
#include "storage/sql/SqlConnectionPool.h"
#include "infra/redis/RedisClient.h"
#include "logger/Logger.h"
namespace infra::health{

HealthService::HealthService()
:startedAt_(std::chrono::steady_clock::now()){

}

HealthService::HealthService(const HealthConfig& config)
:config_(config),startedAt_(std::chrono::steady_clock::now()){

}
void HealthService::setConfig(const HealthConfig& config){
    config_=config;
}
void HealthService::setSqlPool(std::weak_ptr<storage::SqlConnectionPool> sqlPool){
    sqlPool_=std::move(sqlPool);
}
void HealthService::setMessageSqlPool(std::weak_ptr<storage::SqlConnectionPool> sqlPool){
    messageSqlPool_=std::move(sqlPool);
}


void HealthService::setOnlineConnectionProvider(std::function<size_t()> provider){
    onlineConnectionProvider_=std::move(provider);
}
void HealthService::setMaintenanceProvider(std::function<infra::maintenance::MaintenanceSnapshot()> provider,int64_t expectedIntervalMs){
    maintenanceProvider_=std::move(provider);
    if(expectedIntervalMs>0){
        maintenanceIntervalMs_=expectedIntervalMs;
    }
}
void HealthService::setMessageExecutorStatsProvider(MessageExecutorStatsProvider provider,std::uint32_t queueWarnPercent) {
    messageExecutorStatsProvider_ =std::move(provider);
    messageQueueWarnPercent_ =queueWarnPercent;
}
void HealthService::setDbReadExecutorStatsProvider(DbReadExecutorStatsProvider provider,std::uint32_t queueWarnPercent){
    dbReadExecutorStatsProvider_=std::move(provider);
    dbReadQueueWarnPercent_=queueWarnPercent;
}
void HealthService::setRedisExecutorStatsProvider(RedisExecutorStatsProvider provider,std::uint32_t queueWarnPercent){
    redisExecutorStatsProvider_=std::move(provider);
    redisQueueWarnPercent_=queueWarnPercent;
}
void HealthService::setRedisProbeProvider(RedisProbeProvider provider){
    redisProbeProvider_=std::move(provider);
}

HealthSnapshot HealthService::snapshot(){
    HealthSnapshot snapshot;
    fillRuntimeStats(snapshot);
    checkSql(snapshot);
    checkMessageSql(snapshot);

    fillLoggerStats(snapshot);
    fillMaintenanceStats(snapshot);
    fillMessageExecutorStats(snapshot);
    fillDbReadExecutorStats(snapshot);
    fillRedisExecutorStats(snapshot);
    fillRedisProbeState(snapshot);
    decideStatus(snapshot);
    return snapshot;
}
void HealthService::checkSql(HealthSnapshot& snapshot){
    auto sqlPool=sqlPool_.lock();
    if(!sqlPool){
        snapshot.sqlEnabled=false;
        snapshot.sqlHealthy=true;
        return;
    }
    else{
        snapshot.sqlEnabled=true;
        auto stats=sqlPool->stats();
        snapshot.sqlStats=stats;
        if(!stats.started){
            snapshot.sqlHealthy=false;
        }
        if(stats.total==0){
            snapshot.sqlHealthy=false;
        }
        if(config_.sqlTimeoutDeltaMode()){
            if(hasNewSqlAcquireTimeouts(stats)){
                snapshot.sqlAcquireTimeoutIncreased=true;
            }
        }
        else{
            if(stats.acquireTimeouts>0){
                snapshot.sqlAcquireTimeoutIncreased=true;
            }
        }
    }
}
void HealthService::checkMessageSql(HealthSnapshot& snapshot){
    auto sqlPool=messageSqlPool_.lock();
    if(!sqlPool){
        snapshot.messageSqlEnabled=false;
        snapshot.messageSqlHealthy=true;
        return;
    }
    
    snapshot.messageSqlEnabled=true;
    snapshot.messageSqlStats=sqlPool->stats();

    const auto& stats=snapshot.messageSqlStats;
    snapshot.messageSqlHealthy=stats.started&&stats.total>0;
    if(config_.sqlTimeoutDeltaMode()){
        snapshot.messageSqlAcquireTimeoutIncreased=stats.acquireTimeouts>lastMessageSqlAcquireTimeouts_;
        lastMessageSqlAcquireTimeouts_=stats.acquireTimeouts;
    }
    else{
        snapshot.messageSqlAcquireTimeoutIncreased=stats.acquireTimeouts>0;
    }
}
bool HealthService::hasNewSqlAcquireTimeouts(const storage::SqlConnectionPoolStats& stats){
    if(stats.acquireTimeouts>lastSqlAcquireTimeouts_){
        lastSqlAcquireTimeouts_=stats.acquireTimeouts;
        return true;
    }
    return false;
}

int64_t HealthService::currentEpochMs() const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void HealthService::fillRuntimeStats(HealthSnapshot& snapshot){
    auto now=std::chrono::steady_clock::now();
    snapshot.uptimeMs=std::chrono::duration_cast<std::chrono::milliseconds>(now-startedAt_).count();
    if(onlineConnectionProvider_){
        snapshot.onlineConnections=onlineConnectionProvider_();
    }
}
void HealthService::fillLoggerStats(HealthSnapshot& snapshot){
    auto stats=Logger::instance().stats();
    snapshot.loggerStats=stats;
}

void HealthService::fillMaintenanceStats(HealthSnapshot& snapshot){
    if(!maintenanceProvider_){
        snapshot.maintenanceEnabled=false;
        snapshot.maintenanceHealthy=true;
        return;
    }

    snapshot.maintenanceEnabled=true;
    auto maintenanceSnapshot=maintenanceProvider_();
    snapshot.maintenance=maintenanceSnapshot;
    //判断从未运行
    
    if(!maintenanceSnapshot.hasRun){
        const uint64_t maintenanceThresholdMs =2ULL * static_cast<uint64_t>(maintenanceIntervalMs_);
        if(snapshot.uptimeMs>maintenanceThresholdMs){//服务运行时间超过两个维护周期
            snapshot.maintenanceStale=true;
        }
    }
    //判断最近执行失败
    
    if(maintenanceSnapshot.hasRun&&!maintenanceSnapshot.lastRunOk){
        snapshot.maintenanceHealthy=false;
    }
    //判断长期没有成功
    const int64_t nowMs=currentEpochMs();
    if(maintenanceSnapshot.lastSuccessAtMs>0&&maintenanceIntervalMs_>0){
        const uint64_t maintenanceThresholdMs =3LL *maintenanceIntervalMs_;
        if(nowMs>=maintenanceSnapshot.lastSuccessAtMs){
            const auto elapsedMs = static_cast<uint64_t>(nowMs - maintenanceSnapshot.lastSuccessAtMs);
            snapshot.maintenanceStale =elapsedMs > maintenanceThresholdMs;
        }
    }
    //判断运行时间过长
    if(maintenanceSnapshot.running){
        const int64_t maintenanceThresholdMs =2ULL * static_cast<uint64_t>(maintenanceIntervalMs_);
        if((currentEpochMs()-maintenanceSnapshot.lastRunAtMs)>maintenanceThresholdMs){
            snapshot.maintenanceRunningTooLong=true;
        }
    }
    snapshot.maintenanceHealthy= !snapshot.maintenanceStale &&!snapshot.maintenanceRunningTooLong && (!snapshot.maintenance.hasRun || snapshot.maintenance.lastRunOk);

}
void HealthService::fillMessageExecutorStats(HealthSnapshot& snapshot) {
    if (!messageExecutorStatsProvider_) {
        snapshot.messageExecutorEnabled = false;
        snapshot.messageExecutorHealthy = true;
        return;
    }

    snapshot.messageExecutorEnabled = true;
    snapshot.messageExecutorStats =messageExecutorStatsProvider_();

    const auto& stats =snapshot.messageExecutorStats;

    if (stats.queueCapacity > 0) {
        const std::size_t warnSize =(stats.queueCapacity *messageQueueWarnPercent_ + 99) / 100;
        snapshot.messageExecutorSaturated =stats.queuedTasks >= warnSize;
    }

    snapshot.messageExecutorRejectedIncreased =stats.rejectedFull >lastMessageRejectedFull_;

    lastMessageRejectedFull_ =stats.rejectedFull;

    snapshot.messageExecutorHealthy =!snapshot.messageExecutorSaturated &&!snapshot.messageExecutorRejectedIncreased &&stats.state ==infra::thread::ThreadPoolState::Running;
}

void HealthService::fillDbReadExecutorStats(HealthSnapshot& snapshot){
if (!dbReadExecutorStatsProvider_) {
        snapshot.dbReadExecutorEnabled = false;
        snapshot.dbReadExecutorHealthy = true;
        return;
    }

    snapshot.dbReadExecutorEnabled = true;
    snapshot.dbReadExecutorStats=dbReadExecutorStatsProvider_();

    const auto& stats =snapshot.dbReadExecutorStats;

    if (stats.queueCapacity > 0) {
        const std::size_t warnSize =(stats.queueCapacity *dbReadQueueWarnPercent_+ 99) / 100;
        snapshot.dbReadExecutorSaturated =stats.queuedTasks >= warnSize;
    }

    snapshot.dbReadExecutorRejectedIncreased =stats.rejectedFull >lastDbReadRejectedFull_;

    lastDbReadRejectedFull_ =stats.rejectedFull;

    snapshot.dbReadExecutorHealthy=!snapshot.dbReadExecutorSaturated &&!snapshot.dbReadExecutorRejectedIncreased &&stats.state ==infra::thread::ThreadPoolState::Running;
}

void HealthService::fillRedisExecutorStats( HealthSnapshot& snapshot) {

    if (!redisExecutorStatsProvider_) {
        snapshot.redisExecutorEnabled = false;
        snapshot.redisExecutorHealthy = true;
        return;
    }

    snapshot.redisExecutorEnabled = true;
    snapshot.redisExecutorStats =redisExecutorStatsProvider_();

    const auto& stats =snapshot.redisExecutorStats;
    if (stats.queueCapacity > 0) {
        const std::size_t warnSize =(stats.queueCapacity *redisQueueWarnPercent_ + 99) / 100;
        snapshot.redisExecutorSaturated =stats.queuedTasks >= warnSize;
    }

    snapshot.redisExecutorRejectedIncreased =stats.rejectedFull >lastRedisRejectedFull_;
    lastRedisRejectedFull_ =stats.rejectedFull;

    snapshot.redisExecutorHealthy =stats.state ==infra::thread::ThreadPoolState::Running &&!snapshot.redisExecutorSaturated &&!snapshot.redisExecutorRejectedIncreased;
}
void HealthService::fillRedisProbeState(HealthSnapshot& snapshot){
    if(!redisProbeProvider_){
        snapshot.redisEnabled=false;
        snapshot.redisProbeHasResult=false;
        snapshot.redisHealthy=true;
        return;
    }
    const auto stats=redisProbeProvider_();
    snapshot.redisEnabled=true;
    snapshot.redisProbeHasResult=stats.hasResult;
    snapshot.redisProbeInFlight=stats.inFlight;
    snapshot.redisProbeLatencyUs=stats.lastLatencyUs;
    snapshot.redisProbeFailedChecks=stats.failedChecks;
    snapshot.redisPingChecked=stats.hasResult;
    snapshot.redisHealthy=stats.hasResult&&stats.healthy;
    snapshot.redisProbeLastCheckAtMs = stats.lastCheckAtMs;

    if (config_.redisPingEnabled()&&stats.hasResult) {
        const auto maxAgeMs =static_cast<std::int64_t>(config_.logIntervalMs()) * 2;
        snapshot.redisProbeStale =currentEpochMs() - stats.lastCheckAtMs > maxAgeMs;
        if (snapshot.redisProbeStale) {
            snapshot.redisHealthy = false;
        }
    }
}
void HealthService::decideStatus(HealthSnapshot& snapshot){
    snapshot.status = HealthStatus::Healthy;

    if (snapshot.sqlEnabled && !snapshot.sqlHealthy) {//SQL不健康
        snapshot.status = HealthStatus::Unhealthy;
        addReason(snapshot,"sql unhealthy");
        return;
    }

    if (snapshot.redisEnabled && !snapshot.redisHealthy) {//Redis不健康
        snapshot.status = HealthStatus::Degraded;
        addReason(snapshot,"redis unhealthy");
    }

    if ( snapshot.sqlAcquireTimeoutIncreased) {//SQL获取连接超时
        snapshot.status = HealthStatus::Degraded;
        addReason(snapshot,"sql acquire timeout occurred");
    }

    if (snapshot.messageSqlEnabled &&!snapshot.messageSqlHealthy) {
        snapshot.status = HealthStatus::Unhealthy;
        addReason(snapshot, "message sql unhealthy");
    }

    if (snapshot.messageSqlAcquireTimeoutIncreased) {
        if (snapshot.status != HealthStatus::Unhealthy) {
            snapshot.status = HealthStatus::Degraded;
        }
        addReason(snapshot,"message sql acquire timeout occurred");
    }

    //维护任务异常
    if(snapshot.maintenanceEnabled&&!snapshot.maintenanceHealthy){
        if(snapshot.maintenance.hasRun&&!snapshot.maintenance.lastRunOk){
            addReason(snapshot,"maintenance last run failed");
    }
        snapshot.status=HealthStatus::Degraded;
    }
    if(snapshot.maintenanceStale){
        addReason(snapshot,"maintenance stale");
        snapshot.status=HealthStatus::Degraded;
    }
    if(snapshot.maintenanceRunningTooLong){
        addReason(snapshot,"maintenance running too long");
        snapshot.status=HealthStatus::Degraded;
    }

    if(snapshot.loggerStats.dropped>0){
        addReason(snapshot,"logger dropped");
    }

    if (snapshot.messageExecutorEnabled &&!snapshot.messageExecutorHealthy) {
        snapshot.status = HealthStatus::Degraded;
        if (snapshot.messageExecutorSaturated) {
            addReason(snapshot,"message executor queue saturated");
        }
        if (snapshot.messageExecutorRejectedIncreased) {addReason(snapshot,"message executor rejected task");
        }
    }

    if (snapshot.dbReadExecutorEnabled &&!snapshot.dbReadExecutorHealthy) {
        snapshot.status = HealthStatus::Degraded;
        if (snapshot.dbReadExecutorSaturated) {
            addReason(snapshot,"dbRead executor queue saturated");
        }
        if (snapshot.dbReadExecutorRejectedIncreased) {
            addReason(snapshot,"dbRead executor rejected task");
        }
    }
    if (snapshot.redisExecutorEnabled &&!snapshot.redisExecutorHealthy) {
        if (snapshot.status !=HealthStatus::Unhealthy) {
            snapshot.status =HealthStatus::Degraded;
        }
        if (snapshot.redisExecutorSaturated) {
            addReason(snapshot,"redis executor queue saturated");
        }
        if (snapshot.redisExecutorRejectedIncreased) {
            addReason(snapshot,"redis executor rejected task");
        }
    }
    if (snapshot.redisProbeStale) {
        snapshot.status = HealthStatus::Degraded;
        addReason(snapshot, "redis probe stale");
    }
}


void HealthService::addReason(HealthSnapshot& snapshot, std::string reason){
    if(snapshot.reason.empty()){
        snapshot.reason=reason;
    }
    else{
        snapshot.reason=snapshot.reason+"; "+reason;
    }
}
}