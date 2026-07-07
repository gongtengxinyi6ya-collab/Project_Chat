#pragma once
#include <string>
#include <cstdint>
#include "third_party/json.hpp"
/*统一管理健康检查开关，周期，Redis探活，SQL timeout*/
class HealthConfig {
public:
    static HealthConfig fromJson(const nlohmann::json& j);
    void loadFromEnv();
    void validateOrThrow() const;

    bool enabled() const{return enabled_;}
    uint32_t logIntervalMs() const{return logIntervalMs_;}
    bool redisPingEnabled() const{return redisPingEnabled_;}
    bool sqlTimeoutDeltaMode() const{return sqlTimeoutDeltaMode_;}

private:
    bool enabled_{true};
    uint32_t logIntervalMs_{30000};
    bool redisPingEnabled_{true};
    bool sqlTimeoutDeltaMode_{true};
};