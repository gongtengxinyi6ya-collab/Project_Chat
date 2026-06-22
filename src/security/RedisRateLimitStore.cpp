#include "security/RedisRateLimitStore.h"
#include "infra/redis/RedisClient.h"

namespace security{

RedisRateLimitStore::RedisRateLimitStore(std::shared_ptr<infra::redis::RedisClient> redis,std::string prefix)
:redis_(std::move(redis)),prefix_(std::move(prefix)){

}

RateLimitResult RedisRateLimitStore::hit(const std::string&key,const RateLimitRule& rule,[[maybe_unused]]int64_t nowMs){
    if(!redis_||key.empty()||rule.maxRequests==0||rule.windowMs<=0){
        return {.allowed=true};
    }
    std::string counterKey=prefix_+"cnt:"+rule.name+":"+key;
    std::string blockKey=prefix_+"blk:"+rule.name+":"+key;
    //检查blockKey是否存在
    if(redis_->exists(blockKey)){
        auto retryAfterMs=redis_->pttl(blockKey);
        if(retryAfterMs<=0){
            if(rule.blockMs>0){
                retryAfterMs=rule.blockMs;
            }
            else{
                retryAfterMs=rule.windowMs;
            }
        }
        return RateLimitResult{.allowed=false,.retryAfterMs=retryAfterMs};
    }
    auto count=redis_->incr(counterKey);
    if(!count){
        return RateLimitResult{.allowed=true};
    }
    if(count.value()==1){
        redis_->pexpire(counterKey,rule.windowMs);
    }
    if(count.value()<=static_cast<int64_t>(rule.maxRequests)){
        auto remaining=rule.maxRequests-static_cast<size_t>(count.value());
        return RateLimitResult{.allowed=true,.remaining=remaining};
    }
    if(rule.blockMs>0){
        redis_->setPx(blockKey,"1",rule.blockMs);
        return RateLimitResult{.allowed=false,.retryAfterMs=rule.blockMs};
    }
    auto retryAfterMs=redis_->pttl(counterKey);
    if(retryAfterMs<=0){
        retryAfterMs=rule.windowMs;
    }
    return RateLimitResult{.allowed=false,.retryAfterMs=retryAfterMs};
}
void RedisRateLimitStore::reset(const std::string& key,const RateLimitRule& rule){
    if(!redis_||key.empty()||rule.name.empty()){
        return ;
    }
    std::string counterKey=prefix_+"cnt:"+rule.name+":"+key;
    std::string blockKey=prefix_+"blk:"+rule.name+":"+key;
    redis_->del(counterKey);
    redis_->del(blockKey);
}
}