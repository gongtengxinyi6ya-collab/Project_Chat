#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include "security/RateLimitKeyType.h"
/*业务层限流*/

namespace security{
class RateLimitStore;

class RateLimiter{
public:
    RateLimiter(std::shared_ptr<RateLimitStore> store);
    RateLimitResult checkRegister(const std::string& ip, int64_t nowMs);//注册接口，按ip限流
    RateLimitResult checkLoginFail(const std::string& accountId, int64_t nowMs);//登录失败限流：按账号限流
    void resetLoginFail(const std::string& accountId);////登录成功后清除失败计数
    RateLimitResult checkSendMessage(const std::string& accountId, int64_t nowMs);//发消息限流，按账号
    RateLimitResult checkSync(const std::string& accountId, int64_t nowMs);//按账号同步请求限流
    RateLimitResult checkHistory(const std::string& accountId, int64_t nowMs);//按账号历史消息分页限流

private:
    std::shared_ptr<RateLimitStore> store_;//限流状态储存
    RateLimitRule loginFailRule_;//登录失败限制
    RateLimitRule registerRule_;//注册频率限制
    RateLimitRule sendMessageRule_;//私聊/群聊发送频率限制
    RateLimitRule syncRule_;//同步请求频率限制
    RateLimitRule historyRule_;//历史消息分页频率限制
};
}