#include "config/DatabaseConfig.h"

DatabaseConfig DatabaseConfig::fromJson(const nlohmann::json& j){
    DatabaseConfig databaseConfig;
    databaseConfig.host_=ConfigParseHelper::getOrDefault(j,"host",databaseConfig.host_);
    databaseConfig.port_=ConfigParseHelper::getOrDefault(j,"port",databaseConfig.port_);
    databaseConfig.user_=ConfigParseHelper::getOrDefault(j,"user",databaseConfig.user_);
    databaseConfig.password_=ConfigParseHelper::getOrDefault(j,"password",databaseConfig.password_);
    databaseConfig.database_=ConfigParseHelper::getOrDefault(j,"database",databaseConfig.database_);
    databaseConfig.poolSize_=ConfigParseHelper::getOrDefault(j,"pool_size",databaseConfig.poolSize_);
    databaseConfig.connectTimeoutMs_=ConfigParseHelper::getOrDefault(j,"connect_timeout_ms",databaseConfig.connectTimeoutMs_);
    databaseConfig.acquireTimeoutMs_ =ConfigParseHelper::getOrDefault(j, "acquire_timeout_ms", databaseConfig.acquireTimeoutMs_);
    return databaseConfig;

}
void DatabaseConfig::loadFromEnv(){
    auto envHost=ConfigParseHelper::getEnv("DB_HOST");
    if(envHost.has_value()){
        host_=envHost.value();
    }
    auto envPort=ConfigParseHelper::getEnv("DB_PORT");
    if(envPort.has_value()){
        port_=ConfigParseHelper::parseEnvUInt(envPort.value(), "DB_PORT", 65535);
    }
    auto envUser=ConfigParseHelper::getEnv("DB_USER");
    if(envUser.has_value()){
        user_=envUser.value();
    }
    auto envPassword=ConfigParseHelper::getEnv("DB_PASSWORD");
    if(envPassword.has_value()){
        password_=envPassword.value();
    }
    auto envDatabase=ConfigParseHelper::getEnv("DB_DATABASE");
    if(envDatabase.has_value()){
        database_=envDatabase.value();
    }
    auto envPoolSize=ConfigParseHelper::getEnv("DB_POOL_SIZE");
    if(envPoolSize.has_value()){
        poolSize_=ConfigParseHelper::parseEnvUInt(envPoolSize.value(), "DB_POOL_SIZE", 1000);
    }
    auto envConnectTimeoutMs=ConfigParseHelper::getEnv("DB_CONNECT_TIMEOUT_MS");
    if(envConnectTimeoutMs.has_value()){
        connectTimeoutMs_=ConfigParseHelper::parseEnvUInt(envConnectTimeoutMs.value(), "DB_CONNECT_TIMEOUT_MS", 60000);
    }
    auto envAcquireTimeoutMs = ConfigParseHelper::getEnv("DB_ACQUIRE_TIMEOUT_MS");
    if (envAcquireTimeoutMs.has_value()) {
        acquireTimeoutMs_ =
            ConfigParseHelper::parseEnvUInt(envAcquireTimeoutMs.value(), "DB_ACQUIRE_TIMEOUT_MS", 60000);
    }
}
void DatabaseConfig::validate() const{
    if(host_.empty()){
        throw std::runtime_error("Database host cannot be empty");
    }
    if(user_.empty()){
        throw std::runtime_error("Database user cannot be empty");
    }
    if(database_.empty()){
        throw std::runtime_error("Database name cannot be empty");
    }
    ConfigParseHelper::checkRange("port", port_, 1, 65535);
    ConfigParseHelper::checkRange("pool_size", poolSize_, 1, 1000);
    ConfigParseHelper::checkRange("connect_timeout_ms", connectTimeoutMs_, 1000, 60000);
    ConfigParseHelper::checkRange("acquire_timeout_ms", acquireTimeoutMs_, 100, 60000);
}
const std::string& DatabaseConfig::host() const{
    return host_;
}
uint16_t DatabaseConfig::port() const{
    return port_;
}
const std::string& DatabaseConfig::user() const{
    return user_;
}
const std::string& DatabaseConfig::password() const{
    return password_;
}
const std::string& DatabaseConfig::database()const{
    return database_;
}
uint32_t DatabaseConfig::poolSize()const{
    return poolSize_;
}
uint32_t DatabaseConfig::connectTimeoutMs()const{
    return connectTimeoutMs_;
}
uint32_t DatabaseConfig::acquireTimeoutMs() const{
    return acquireTimeoutMs_;
}