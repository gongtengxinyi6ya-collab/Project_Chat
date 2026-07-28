#include "config/AuthAsyncConfig.h"
#include "config/ConfigParseHelper.h"
AuthAsyncConfig AuthAsyncConfig::fromJson(const nlohmann::json& j){
    AuthAsyncConfig config;
    config.enabled=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled);
    config.workerThreads=ConfigParseHelper::getOrDefault(j,"worker_threads",config.workerThreads);
    config.queueCapacityPerShard=ConfigParseHelper::getOrDefault(j,"queue_capacity_per_shard",config.queueCapacityPerShard);
    config.queueWarnPercent=ConfigParseHelper::getOrDefault(j,"queue_warn_percent",config.queueWarnPercent);
    return config;
}

void AuthAsyncConfig::applyEnvOverrides(){
    auto envEnabled=ConfigParseHelper::getEnv("AUTH_ASYNC_ENABLED");
    if(envEnabled.has_value()){
        enabled=ConfigParseHelper::parseEnvBool(envEnabled.value(), "AUTH_ASYNC_ENABLED");
    }
    auto envthread=ConfigParseHelper::getEnv("AUTH_ASYNC_WORKER_THREADS");
    if(envthread.has_value()){
        workerThreads=ConfigParseHelper::parseEnvUInt64(envthread.value(), "AUTH_ASYNC_WORKER_THREADS");
    }
    auto envqueueCapacity=ConfigParseHelper::getEnv("AUTH_ASYNC_QUEUE_CAPACITY_PER_SHARD");
    if(envqueueCapacity.has_value()){
        queueCapacityPerShard=ConfigParseHelper::parseEnvUInt64(envqueueCapacity.value(), "AUTH_ASYNC_QUEUE_CAPACITY_PER_SHARD");
    }
    auto envqueueWarn=ConfigParseHelper::getEnv("AUTH_ASYNC_QUEUE_WARN_PERCENT");
    if(envqueueWarn.has_value()){
        queueWarnPercent=ConfigParseHelper::parseEnvUInt(envqueueWarn.value(), "AUTH_ASYNC_QUEUE_WARN_PERCENT");
    }
}

void AuthAsyncConfig::validateOrThrow()const{
    ConfigParseHelper::checkRange("worker_threads",workerThreads,1,32);
    ConfigParseHelper::checkRange("queue_capacity_per_shard",queueCapacityPerShard,1,100000);
    ConfigParseHelper::checkRange("queue_warn_percent",queueWarnPercent,1,100);
}