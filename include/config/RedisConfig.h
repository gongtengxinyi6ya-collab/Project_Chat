#pragma once
#include <string>
#include <cstdint>
#include "third_party/json.hpp"

class RedisConfig {
public:
    static RedisConfig fromJson(const nlohmann::json& j);//读取配置
    void loadFromEnv();//加载环境变量
    void validateOrThrow() const;//校验数值

    bool enabled() const{return enabled_;}
    const std::string& host() const{return host_;}
    uint16_t port() const{return port_;};
    const std::string& password() const{return password_;};
    int db() const{return db_;};
    size_t poolSize() const{return poolSize_;};
    int connectTimeoutMs() const{return connectTimeoutMs_;};
    int socketTimeoutMs() const{return socketTimeoutMs_;};
    const std::string& keyPrefix() const{return keyPrefix_;};

private:
    bool enabled_{false};
    std::string host_{"127.0.0.1"};
    uint16_t port_{6379};
    std::string password_{};
    int db_{0};
    size_t poolSize_{4};
    int connectTimeoutMs_{3000};
    int socketTimeoutMs_{3000};
    std::string keyPrefix_{"project_chat:"};
};