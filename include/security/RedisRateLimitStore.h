#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include "security/RateLimitStore.h"

/*实现抽象类，用Redis回鹘限流窗口和封禁状态
负责根据规则名和业务key生产Redis key
检查是否处于封禁状态
对窗口计数器自增
第一次访问时设置窗口TTL
超过阈值设置block key 
Redis异常时失败放行*/
namespace infra::redis{
    class RedisClient;
}
namespace security{
class RedisRateLimitStore:public RateLimitStore{
public:
    RedisRateLimitStore(std::shared_ptr<infra::redis::RedisClient> redis,std::string prefix = "project_chat:rate:");
    RateLimitResult hit(const std::string& key,const RateLimitRule& rule,int64_t nowMs)override;//记录一次访问，并判断是否超限
    void reset(const std::string& key,const RateLimitRule &rule)override;//删除计数key，block key
private:
    std::shared_ptr<infra::redis::RedisClient> redis_;
    std::string prefix_{"project_chat:rate:"};//限流key同一前缀
};
}