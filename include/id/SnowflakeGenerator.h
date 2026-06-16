#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <chrono>
namespace snowflakeId{
class SnowflakeIdGenerator{
public:
    SnowflakeIdGenerator(uint16_t nodeId, uint64_t epochMs);
    uint64_t nextId();
    std::string nextStringId(std::string_view prefix="G");
private:
    uint16_t nodeId_;//
    uint64_t epochMs_;//起始时间
    uint64_t lastTimestampMs_{0};
    uint16_t sequence_{0};//序号
    std::mutex mutex_;
};
}