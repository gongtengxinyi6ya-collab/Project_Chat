#include "config/RedisConfig.h"
#include "config/ConfigParseHelper.h"
#include <stdexcept>
RedisConfig RedisConfig::fromJson(const nlohmann::json& j){
    RedisConfig config;
    config.enabled_=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled_);
    config.host_=ConfigParseHelper::getOrDefault(j,"host",config.host_);
    config.port_=ConfigParseHelper::getOrDefault(j,"port",config.port_);
    config.password_=ConfigParseHelper::getOrDefault(j,"password",config.password_);
    config.db_=ConfigParseHelper::getOrDefault(j,"db",config.db_);
    config.poolSize_=ConfigParseHelper::getOrDefault(j,"pool_size",config.poolSize_);
    config.connectTimeoutMs_=ConfigParseHelper::getOrDefault(j,"connect_timeout_ms",config.connectTimeoutMs_);
    config.socketTimeoutMs_=ConfigParseHelper::getOrDefault(j,"socket_timeout_ms",config.socketTimeoutMs_);
    config.keyPrefix_=ConfigParseHelper::getOrDefault(j,"key_prefix",config.keyPrefix_);
    return config;
}
void RedisConfig::loadFromEnv(){
    auto envEnabled=ConfigParseHelper::getEnv("REDIS_ENABLED");
    if(envEnabled.has_value()){
        enabled_=ConfigParseHelper::parseEnvBool(envEnabled.value(), "REDIS_ENABLED");
    }
    auto envHost=ConfigParseHelper::getEnv("REDIS_HOST");
    if(envHost.has_value()){
        host_=envHost.value();
    }
    auto envPort=ConfigParseHelper::getEnv("REDIS_PORT");
    if(envPort.has_value()){
        port_=ConfigParseHelper::parseEnvUInt(envPort.value(), "REDIS_PORT", 65535);
    }
    auto envPassword=ConfigParseHelper::getEnv("REDIS_PASSWORD");
    if(envPassword.has_value()){
        password_=envPassword.value();
    }
    auto envDb=ConfigParseHelper::getEnv("REDIS_DB");
    if(envDb.has_value()){
        db_=ConfigParseHelper::parseEnvInt(envDb.value(), "REDIS_DB");
    }
    auto envPoolSize=ConfigParseHelper::getEnv("REDIS_POOL_SIZE");
    if(envPoolSize.has_value()){
        poolSize_=ConfigParseHelper::parseEnvUInt(envPoolSize.value(), "REDIS_POOL_SIZE", 1000);
    }
    auto envConnectTimeoutMs=ConfigParseHelper::getEnv("REDIS_CONNECT_TIMEOUT_MS");
    if(envConnectTimeoutMs.has_value()){
        connectTimeoutMs_=ConfigParseHelper::parseEnvUInt(envConnectTimeoutMs.value(), "REDIS_CONNECT_TIMEOUT_MS", 60000);
    }
    auto envSocketTimeoutMs=ConfigParseHelper::getEnv("REDIS_SOCKET_TIMEOUT_MS");
    if(envSocketTimeoutMs.has_value()){
        socketTimeoutMs_=ConfigParseHelper::parseEnvUInt(envSocketTimeoutMs.value(), "REDIS_SOCKET_TIMEOUT_MS", 60000);
    }
    auto envKeyPrefix=ConfigParseHelper::getEnv("REDIS_KEY_PREFIX");
    if(envKeyPrefix.has_value()){
        keyPrefix_=envKeyPrefix.value();
    }
}
void RedisConfig::validateOrThrow() const{
    if(enabled_){
        if(host_.empty()){
            throw std::runtime_error("Redis host cannot be empty when enabled");
        }
        ConfigParseHelper::checkRange("port", port_, 1, 65535);
        ConfigParseHelper::checkRange("db", db_, 0, 16);
        ConfigParseHelper::checkRange("pool_size", poolSize_, 1, 1000);
        ConfigParseHelper::checkRange("connect_timeout_ms", connectTimeoutMs_, 1000, 60000);
        ConfigParseHelper::checkRange("socket_timeout_ms", socketTimeoutMs_, 1000, 60000);
        if(keyPrefix_.empty()){
            throw std::runtime_error("Redis key prefix cannot be empty when enabled");
        }
    }
}