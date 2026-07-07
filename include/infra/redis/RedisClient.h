#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include "config/RedisConfig.h"

/*Redis访问入口：
负责建立连接；封装常用命令；
捕获redis异常；*/

namespace infra::redis{
class RedisClient{
public:
    explicit RedisClient(const RedisConfig& config);//保存Redis配置
    ~RedisClient();
    bool connect();//根据配置创建Redis连接
    bool connected()const;//返回最近一次连接是否成功
    bool ping();//实时健康检查

    //基础命令
    std::optional<std::string> get(const std::string&key);//读取字符串key
    bool set(const std::string& key,const std::string&value);//写入普通字符串
    bool setEx(const std::string& key,const std::string&value,int64_t ttlSeconds);//写入字符串并设置秒级过期
    bool setPx(const std::string& key,const std::string&value,int64_t ttlMs);//写入字符串并设置毫秒级别过期
    bool del(const std::string& key);//删除单个key
    std::optional<int64_t> incr(const std::string& key);//对key自增，用于限流计数器
    bool expire(const std::string& key,int64_t ttlSeconds);//给key设置秒级过期时间
    bool pexpire(const std::string& key,int64_t ttlMs);//设置毫秒级过期时间
    int64_t pttl(const std::string& key);//获取key剩余过期时间，单位毫秒
    bool exists(const std::string&key);//判断key是否存在
    std::optional<int64_t> evalInt(const std::string& script,const std::vector<std::string>&keys,const std::vector<std::string>& args);//执行Lua脚本，并返回整数结果
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;//Pimpl
};
}