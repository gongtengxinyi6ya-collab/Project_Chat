#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include "ConfigParseHelper.h"
#include "ImConfig.h"
#include "ServerConfig.h"
#include "NetConfig.h"
#include "LogConfig.h"
#include "DatabaseConfig.h"
#include "StorageConfig.h"
#include "IdConfig.h"
#include "RedisConfig.h"
#include "HealthConfig.h"
/*
总配置聚合：统一承载服务端/网络/日志/IM配置，提供加载，校验，查询
*/
class AppConfig{
public:
    static AppConfig loadFromFile(const std::string& path);//从JSON文件加载配置
    void applyEnvOverrides();//允许环境变量覆盖配置
    void validateOrThrow()const;//启动前一次性校验，避免运行期隐患
    std::string dumpSummary()const;//返回一行可读摘要用于启动日志

    //访问器
    const ServerConfig& server()const{return server_;}//只读访问server_
    const NetConfig& net()const{return net_;}//只读访问net_
    const LogConfig& log()const{return log_;}//只读访问log_
    const ImConfig& im()const{return im_;}//只读访问im_
    const DatabaseConfig& database()const{return databaseConfig_;}
    const StorageConfig& storage()const{return storageConfig_;}//获取存储配置
    const IdConfig& id()const{return idConfig_;}//获取ID生成器配置
    const RedisConfig& redis()const{return redisConfig_;}
    const HealthConfig& health()const{return healthConfig_;}
private:
    ServerConfig server_;
    NetConfig net_;
    LogConfig log_;
    ImConfig im_;
    DatabaseConfig databaseConfig_;
    StorageConfig storageConfig_;//存储后端配置
    IdConfig idConfig_;//ID生成器配置
    RedisConfig redisConfig_;//redis配置
    HealthConfig healthConfig_;//健康检查服务配置
};