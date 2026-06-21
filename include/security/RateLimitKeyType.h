#pragma once
#include <string>
#include <cstdint>
namespace security{
enum class RateLimitKeyType {//限流维度
    Ip,//同一ip注册，登录显示
    Account,//同一账号登录失败，发消息限制
    Conn,//同一连接请求频率限制
    Api//全局限制
};

struct RateLimitRule {
    std::string name{};//规则名
    size_t maxRequests{0};//时间窗口内最多允许多少次
    int64_t windowMs{0};//滑动窗口或固定窗口大小
    int64_t blockMs{0};//超限后封禁多久
};

struct RateLimitResult {//限流结果
    bool allowed{true};//请求是否允许通过
    size_t remaining{0};//当前窗口剩余可请求次数
    int64_t retryAfterMs{0};//被限流后还需要等待多久
};


}