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
void HealthService::setRedisClient(std::weak_ptr<infra::redis::RedisClient> redisClient){
    redisClient_=std::move(redisClient);
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
HealthSnapshot HealthService::snapshot(){
    HealthSnapshot snapshot;
    fillRuntimeStats(snapshot);
    checkSql(snapshot);
    checkRedis(snapshot);

    fillLoggerStats(snapshot);
    fillMaintenanceStats(snapshot);
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
bool HealthService::hasNewSqlAcquireTimeouts(const storage::SqlConnectionPoolStats& stats){
    if(stats.acquireTimeouts>lastSqlAcquireTimeouts_){
        lastSqlAcquireTimeouts_=stats.acquireTimeouts;
        return true;
    }
    return false;
}
void HealthService::checkRedis(HealthSnapshot& snapshot){
    auto redisClient=redisClient_.lock();
    if(!redisClient){
        snapshot.redisEnabled=false;
        snapshot.redisHealthy=true;
    }
    else{
        snapshot.redisEnabled=true;
        if(config_.redisPingEnabled()){
            if(!redisClient->ping()){
                snapshot.redisHealthy=false;
            }
            snapshot.redisPingChecked=true;
        }
        else{
            if(!redisClient->connected()){
                snapshot.redisHealthy=false;
    
            }
        }
    }
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
    if(maintenanceSnapshot.lastSuccessAtMs>0){
        const uint64_t maintenanceThresholdMs =3ULL * static_cast<uint64_t>(maintenanceIntervalMs_);
        if((currentEpochMs()-maintenanceSnapshot.lastSuccessAtMs)>maintenanceThresholdMs){
            snapshot.maintenanceStale=true;
        }
    }
    //判断运行时间过长
    if(maintenanceSnapshot.running){
        const uint64_t maintenanceThresholdMs =2ULL * static_cast<uint64_t>(maintenanceIntervalMs_);
        if((currentEpochMs()-maintenanceSnapshot.lastRunAtMs)>maintenanceThresholdMs){
            snapshot.maintenanceRunningTooLong=true;
        }
    }
    snapshot.maintenanceHealthy= !snapshot.maintenanceStale &&!snapshot.maintenanceRunningTooLong && (!snapshot.maintenance.hasRun || snapshot.maintenance.lastRunOk);

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

    //维护任务异常
    if(snapshot.maintenanceEnabled&&!snapshot.maintenanceHealthy){
        addReason(snapshot,"maintenance last run failed");
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