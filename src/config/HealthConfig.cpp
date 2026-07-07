#include "config/HealthConfig.h"
#include "config/ConfigParseHelper.h"
HealthConfig HealthConfig::fromJson(const nlohmann::json& j){
    HealthConfig config;
    config.enabled_=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled_);
    config.logIntervalMs_=ConfigParseHelper::getOrDefault(j,"log_interval_ms",config.logIntervalMs_);
    config.redisPingEnabled_=ConfigParseHelper::getOrDefault(j,"redis_ping_enabled",config.redisPingEnabled_);
    config.sqlTimeoutDeltaMode_=ConfigParseHelper::getOrDefault(j,"sql_timeout_delta_mode",config.sqlTimeoutDeltaMode_);
    return config;
}

void HealthConfig::loadFromEnv(){
    auto envHealthEnabled=ConfigParseHelper::getEnv("HEALTH_ENABLE");
    if(envHealthEnabled.has_value()){
        enabled_=ConfigParseHelper::parseEnvBool(envHealthEnabled.value(), "HEALTH_ENABLE");
    }
    auto envLogIntervalMs=ConfigParseHelper::getEnv("HEALTH_LOG_INTERVAL_MS");
    if(envLogIntervalMs.has_value()){
        logIntervalMs_=ConfigParseHelper::parseEnvUInt(envLogIntervalMs.value(),"HEALTH_LOG_INTERVAL_MS");
    }
    auto envHealthRedisEnabled=ConfigParseHelper::getEnv("HEALTH_REDIS_PING_ENABLE");
    if(envHealthRedisEnabled.has_value()){
        redisPingEnabled_=ConfigParseHelper::parseEnvBool(envHealthRedisEnabled.value(), "HEALTH_REDIS_PING_ENABLE");
    }
    auto envHealthSqlMode=ConfigParseHelper::getEnv("HEALTH_SQL_TIMEOUT_DELTA_MODE");
    if(envHealthSqlMode.has_value()){
        sqlTimeoutDeltaMode_=ConfigParseHelper::parseEnvBool(envHealthSqlMode.value(), "HEALTH_SQL_TIMEOUT_DELTA_MODE");
    }
    
}

void HealthConfig::validateOrThrow()const{
    ConfigParseHelper::checkRange("log_interval_ms",logIntervalMs_,1000,300000);
}