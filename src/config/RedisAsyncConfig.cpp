#include "config/RedisAsyncConfig.h"
#include "config/ConfigParseHelper.h"
RedisAsyncConfig RedisAsyncConfig::fromJson(const nlohmann::json& j){
    RedisAsyncConfig config;
    config.enabled=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled);
    config.workerThreads=ConfigParseHelper::getOrDefault(j,"worker_threads",config.workerThreads);
    config.queueCapacityPerShard=ConfigParseHelper::getOrDefault(j,"queue_capacity_per_shard",config.queueCapacityPerShard);
    config.queueWarnPercent=ConfigParseHelper::getOrDefault(j,"queue_warn_percent",config.queueWarnPercent);
    config.failOpen=ConfigParseHelper::getOrDefault(j,"fail_open",config.failOpen);
    return config;
}

void RedisAsyncConfig::applyEnvOverrides(){
    auto envEnabled=ConfigParseHelper::getEnv("REDIS_ASYNC_ENABLED");
    if(envEnabled.has_value()){
        enabled=ConfigParseHelper::parseEnvBool(envEnabled.value(), "REDIS_ASYNC_ENABLED");
    }
    auto envthread=ConfigParseHelper::getEnv("REDIS_ASYNC_WORKER_THREADS");
    if(envthread.has_value()){
        workerThreads=ConfigParseHelper::parseEnvUInt64(envthread.value(), "REDIS_ASYNC_WORKER_THREADS");
    }
    auto envqueueCapacity=ConfigParseHelper::getEnv("REDIS_ASYNC_QUEUE_CAPACITY");
    if(envqueueCapacity.has_value()){
        queueCapacityPerShard=ConfigParseHelper::parseEnvUInt64(envqueueCapacity.value(), "REDIS_ASYNC_QUEUE_CAPACITY_PER_SHARD");
    }
    auto envqueueWarn=ConfigParseHelper::getEnv("REDIS_ASYNC_QUEUE_WARN_PERCENT");
    if(envqueueWarn.has_value()){
        queueWarnPercent=ConfigParseHelper::parseEnvUInt(envqueueWarn.value(), "REDIS_ASYNC_QUEUE_WARN_PERCENT");
    }
    auto envFailOpen=ConfigParseHelper::getEnv("REDIS_ASYNC_FAIL_OPEN");
    if(envFailOpen.has_value()){
        failOpen=ConfigParseHelper::parseEnvBool(envFailOpen.value(), "REDIS_ASYNC_ENABLED");
    }
}

void RedisAsyncConfig::validateOrThrow()const{
    ConfigParseHelper::checkRange("worker_threads",workerThreads,1,32);
    ConfigParseHelper::checkRange("queuq_capacity",queueCapacityPerShard,1,100000);
    ConfigParseHelper::checkRange("queue_warn_percent",queueWarnPercent,1,100);
}