#pragma once
#include <cstdint>
#include <string>
#include "third_party/json.hpp"

class IdConfig {
public:
    static IdConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;

    uint16_t snowflakeNodeId{0};
    uint64_t snowflakeEpochMs{1735689600000};
};