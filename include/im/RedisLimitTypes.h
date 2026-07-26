#pragma once
#include "security/rate_limit/RateLimitKeyType.h"
#include <string>
#include <cstdint>

namespace im{
enum class RateLimitAction : std::uint8_t {//限流动作类型
    SendMessage,
    History,
    Sync
};

struct AsyncRateLimitResult {//异步限流结果
    security::RateLimitResult decision{};
    std::int64_t queueWaitUs{0};
    std::int64_t executeUs{0};
    std::string exceptionMessage{};
};
}