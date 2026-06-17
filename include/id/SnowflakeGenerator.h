#pragma once
#include <cstdint>
#include <mutex>
#include <string>

namespace snowflakeId{
class SnowflakeIdGenerator{
public:
    SnowflakeIdGenerator(uint16_t nodeId, uint64_t epochMs);
    uint64_t nextId();
    std::string nextStringId(const std::string& prefix="G");
private:
    uint16_t nodeId_{0};//
    uint64_t epochMs_{0};//起始时间
    uint64_t lastTimestampMs_{0};
    uint16_t sequence_{0};//序号
    std::mutex mutex_;

    //位分配
    static constexpr uint8_t kNodeIdBits_{10};
    static constexpr uint8_t kSequenceBits_{12};
    static constexpr uint8_t kNodeIdShirft_{kSequenceBits_};
    static constexpr uint8_t kTimestampShirft_{kNodeIdBits_+kSequenceBits_};
    uint64_t currentMs()const;

};
}