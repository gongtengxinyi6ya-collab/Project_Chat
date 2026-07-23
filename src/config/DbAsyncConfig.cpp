#include "config/DbAsyncConfig.h"
#include "config/ConfigParseHelper.h"
DbAsyncConfig DbAsyncConfig::fromJson(const nlohmann::json& j){
    DbAsyncConfig config;
    config.enabled=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled);
    config.workerThreads=ConfigParseHelper::getOrDefault(j,"worker_threads",config.workerThreads);
    config.queueCapacity=ConfigParseHelper::getOrDefault(j,"queue_capacity",config.queueCapacity);
    config.queueWarnPercent=ConfigParseHelper::getOrDefault(j,"queue_warn_percent",config.queueWarnPercent);
    return config;
}

