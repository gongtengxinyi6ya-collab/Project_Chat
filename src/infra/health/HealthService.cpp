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
void HealthService::setMaintenanceProvider(std::function<infra::maintenance::MaintenanceSnapshot()> provider){
    maintenanceProvider_=std::move(provider);
}
HealthSnapshot HealthService::snapshot(){
    HealthSnapshot snapshot;
    checkSql(snapshot);
    checkRedis(snapshot);
    fillRuntimeStats(snapshot);
    fillLoggerStats(snapshot);
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
                snapshot.status=HealthStatus::Degraded;
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
                addReason(snapshot,"redis unhealthy");
            }
            snapshot.redisPingChecked=true;
        }
        else{
            if(!redisClient->connected()){
                snapshot.redisHealthy=false;
                addReason(snapshot,"redis unhealthy");
                snapshot.status=HealthStatus::Degraded;
            }
        }
    }
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
    if(stats.dropped>0){
        addReason(snapshot,"Logger dropped");
    }
}
void HealthService::decideStatus(HealthSnapshot& snapshot){
    snapshot.status = HealthStatus::Healthy;

    if (snapshot.sqlEnabled && !snapshot.sqlHealthy) {
        snapshot.status = HealthStatus::Unhealthy;
        addReason(snapshot,"sql unhealthy");
        return;
    }

    if (snapshot.redisEnabled && !snapshot.redisHealthy) {
        snapshot.status = HealthStatus::Degraded;
        addReason(snapshot,"redis unhealthy");
    }

    if ( snapshot.sqlAcquireTimeoutIncreased) {
        snapshot.status = HealthStatus::Degraded;
        addReason(snapshot,"sql acquire timeout occurred");
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