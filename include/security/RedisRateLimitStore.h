#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include "security/RateLimitStore.h"

/*实现抽象类，用Redis回鹘限流窗口和封禁状态*/
namespace infra::redis{
    class RedisClient;
}
namespace security{
class RedisRateLimitStore:public RateLimitStore{
public:
    RedisRateLimitStore(std::shared_ptr<infra::redis::RedisClient> redis,std::string prefix = "project_chat:rate:");
    RateLimitResult hit(const std::string& key,const RateLimitRule& rule,int64_t nowMs) override;
    void reset(const std::string& key) override;
private:
    std::shared_ptr<infra::redis::RedisClient> redis_;
    std::string prefix_{"project_chat:rate:"};
};
}