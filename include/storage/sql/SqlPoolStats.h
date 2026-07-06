#pragma once
#include <string>
#include <cstdint>
namespace storage{
struct SqlConnectionPoolStats {
    size_t total{0};//连接池总连接数
    size_t idle{0};//当前空闲连接数
    size_t inUse{0};//正在被业务使用的连接数
    uint64_t acquireTimeouts{0};//获取链接超时次数
    uint64_t reconnects{0};//连接重建次数
    uint64_t replaceFailures{0};//
    uint64_t acquireCount{0};
    bool started{false};
    uint32_t acquireTimeoutMs{0};
};
    std::string formatSqlPoolStats(const SqlConnectionPoolStats& stats);
}