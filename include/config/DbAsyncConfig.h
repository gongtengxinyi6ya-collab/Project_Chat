#pragma once

#include <cstddef>
#include <cstdint>

#include "third_party/json.hpp"

class DbAsyncConfig {
public:
    static DbAsyncConfig fromJson(const nlohmann::json& json);

    void applyEnvOverrides();
    void validateOrThrow() const;

    bool enabled{true};
    std::size_t workerThreads{4};
    std::size_t queueCapacity{2048};
    std::uint32_t queueWarnPercent{80};
};