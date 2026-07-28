#pragma once

#include <cstddef>
#include <cstdint>
#include "third_party/json.hpp"

class AuthAsyncConfig {
public:
    static AuthAsyncConfig fromJson(const nlohmann::json& json);
    void applyEnvOverrides();
    void validateOrThrow() const;

    bool enabled{true};
    std::size_t workerThreads{2};
    std::size_t queueCapacityPerShard{256};
    std::uint32_t queueWarnPercent{80};
};