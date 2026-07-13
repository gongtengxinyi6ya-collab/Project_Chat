#include "config/MessageAsyncConfig.h"
#include "config/ConfigParseHelper.h"

MessageAsyncConfig MessageAsyncConfig::fromJson(const nlohmann::json& j){
    MessageAsyncConfig config;
    config.enabled=ConfigParseHelper::getOrDefault(j,"enable",config.enabled);
    config.workerThreads=ConfigParseHelper::getOrDefault(j,"worker_threads",config.workerThreads);
    config.queueCapacity=ConfigParseHelper::getOrDefault(j,"queue_capacity",config.queueCapacity);
    config.queueWarnPercent=ConfigParseHelper::getOrDefault(j,"queue_warn_percent",config.queueWarnPercent);
    return config;
}

void MessageAsyncConfig::applyEnvOverrides(){
    auto envEnabled=ConfigParseHelper::getEnv("MESSAGE_ASYNC_ENABLED");
    if(envEnabled.has_value()){
        enabled=ConfigParseHelper::parseEnvBool(envEnabled.value(), "MESSAGE_ASYNC_ENABLED");
    }
    auto envthread=ConfigParseHelper::getEnv("MESSAGE_ASYNC_WORKER_THREADS");
    if(envthread.has_value()){
        workerThreads=ConfigParseHelper::parseEnvUInt64(envthread.value(), "MESSAGE_ASYNC_WORKER_THREADS");
    }
    auto envqueueCapacity=ConfigParseHelper::getEnv("MESSAGE_ASYNC_QUEUE_CAPACITY");
    if(envqueueCapacity.has_value()){
        queueCapacity=ConfigParseHelper::parseEnvUInt64(envqueueCapacity.value(), "MESSAGE_ASYNC_QUEUE_CAPACITY");
    }
    auto envqueueWarn=ConfigParseHelper::getEnv("MESSAGE_ASYNC_QUEUE_WARN_PERCENT");
    if(envqueueWarn.has_value()){
        queueWarnPercent=ConfigParseHelper::parseEnvUInt(envqueueWarn.value(), "MESSAGE_ASYNC_QUEUE_WARN_PERCENT");
    }
}

void MessageAsyncConfig::validateOrThrow()const{
    ConfigParseHelper::checkRange("worker_threads",workerThreads,1,32);
    ConfigParseHelper::checkRange("queuq_capacity",queueCapacity,1,100000);
    ConfigParseHelper::checkRange("queue_warn_percent",queueWarnPercent,1,100);
}