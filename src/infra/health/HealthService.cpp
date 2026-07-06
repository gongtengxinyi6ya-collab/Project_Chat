#include "infra/health/HealthService.h"
#include "storage/sql/SqlConnectionPool.h"
#include "infra/redis/RedisClient.h"

namespace infra::health{

HealthService::HealthService()
:startedAt_(std::chrono::steady_clock::now()){

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
HealthSnapshot HealthService::snapshot(){
    HealthSnapshot snapshot;
    checkSql(snapshot);
    checkRedis(snapshot);
    fillRuntimeStats(snapshot);
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
        if(stats.acquireTimeouts>0){
            snapshot.status=HealthStatus::Degraded;
        }
    }
}
void HealthService::checkRedis(HealthSnapshot& snapshot){
    auto redisClien=redisClient_.lock();
    if(!redisClien){
        snapshot.redisEnabled=false;
        snapshot.redisHealthy=true;
    }
    else{
        snapshot.redisEnabled=true;
        if(!redisClien->connected()){
            snapshot.redisHealthy=false;
            snapshot.status=HealthStatus::Degraded;
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
void HealthService::decideStatus(HealthSnapshot& snapshot){
    snapshot.status = HealthStatus::Healthy;

    if (snapshot.sqlEnabled && !snapshot.sqlHealthy) {
        snapshot.status = HealthStatus::Unhealthy;
        snapshot.reason = "sql unhealthy";
        return;
    }

    if (snapshot.redisEnabled && !snapshot.redisHealthy) {
        snapshot.status = HealthStatus::Degraded;
        snapshot.reason = "redis unhealthy";
    }

    if (snapshot.sqlStats.acquireTimeouts > 0) {
        snapshot.status = HealthStatus::Degraded;
        snapshot.reason = "sql acquire timeout occurred";
    }
}
}