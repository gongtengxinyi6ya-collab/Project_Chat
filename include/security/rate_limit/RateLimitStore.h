#pragma once
#include <string>
#include <cstdint>
#include "security/rate_limit/RateLimitKeyType.h"
namespace security{

class RateLimitStore{
public:
    virtual ~RateLimitStore()=default;
    virtual RateLimitResult hit(const std::string&key,const RateLimitRule& rule,int64_t nowMs)=0;//记录一次访问并判断是否超限
    virtual void reset(const std::string& key,const RateLimitRule& role)=0;//清除某个key的限流状态
};
}