#include "security/rate_limit/RedisRateLimitStore.h"
#include "infra/redis/RedisClient.h"

namespace security{

RedisRateLimitStore::RedisRateLimitStore(std::shared_ptr<infra::redis::RedisClient> redis,std::string prefix)
:redis_(std::move(redis)),prefix_(std::move(prefix)){

}

RateLimitResult RedisRateLimitStore::hit(const std::string&key,const RateLimitRule& rule,[[maybe_unused]]int64_t nowMs){
    if(!redis_||key.empty()||rule.maxRequests==0||rule.windowMs<=0){
        return {.allowed=true};
    }
    return hitByLua(key,rule);
}

void RedisRateLimitStore::reset(const std::string& key,const RateLimitRule& rule){
    if(!redis_||key.empty()||rule.name.empty()){
        return ;
    }
    auto cntKey=counterKey(key,rule);
    std::string blkKey=blockKey(key,rule);
    redis_->del(cntKey);
    redis_->del(blkKey);
}

std::string RedisRateLimitStore::counterKey(const std::string& key, const RateLimitRule& rule) const{
    if(key.empty()||rule.name.empty()){
        return "";
    }
    return prefix_+"cnt:"+rule.name+":"+key;
}
std::string RedisRateLimitStore::blockKey(const std::string& key, const RateLimitRule& rule) const{
    if(key.empty()||rule.name.empty()){
        return "";
    }
    return prefix_+"blk:"+rule.name+":"+key;
}
RateLimitResult RedisRateLimitStore::hitByLua(const std::string& key, const RateLimitRule& rule){
    if(!redis_||key.empty()||rule.name.empty()){
        return {.allowed=true};
    }
    auto cntKey=counterKey(key,rule);
    auto blkKey=blockKey(key,rule);
    auto valueOpt=redis_->evalInt(hitScript(),{cntKey,blkKey},{std::to_string(rule.maxRequests),std::to_string(rule.windowMs),std::to_string(rule.blockMs)});
    if(!valueOpt.has_value()){
        return {.allowed=true};
    }
    if(valueOpt.value()>=0){
        //表示允许通过
        const auto count=static_cast<std::size_t>(valueOpt.value());
        auto remaining=count<rule.maxRequests?rule.maxRequests-count:0;
        return {.allowed=true,.remaining=remaining};
    }
    //被限流
    auto retryAfterMs=-valueOpt.value();
    if(retryAfterMs<=0){
        retryAfterMs=rule.blockMs>0?rule.blockMs:rule.windowMs;
    }
    return {.allowed=false,.retryAfterMs=retryAfterMs};
}
const std::string& RedisRateLimitStore::hitScript(){
    static const std::string script=R"(
    -- KEYS[1] = counterKey
    -- KEYS[2] = blockKey
    -- ARGV[1] = maxRequests
    -- ARGV[2] = windowMs
    -- ARGV[3] = blockMs

    local blocked = redis.call('PTTL', KEYS[2])
    if blocked > 0 then
        return -blocked
    end

    local count = redis.call('INCR', KEYS[1])
    if count == 1 then
        redis.call('PEXPIRE', KEYS[1], ARGV[2])
    end

    if count <= tonumber(ARGV[1]) then
        return count
    end

    local blockMs = tonumber(ARGV[3])
    if blockMs > 0 then
        redis.call('SET', KEYS[2], '1', 'PX', blockMs)
        return -blockMs
    end

    local ttl = redis.call('PTTL', KEYS[1])
    if ttl <= 0 then
        ttl = tonumber(ARGV[2])
    end
    return -ttl
    )";
    return script;
}
}