#include "config/IdConfig.h"
#include "config/ConfigParseHelper.h"
#include <chrono>
#include <stdexcept>
IdConfig IdConfig::fromJson(const nlohmann::json& j) {
    IdConfig config;
    config.snowflakeNodeId=ConfigParseHelper::getOrDefault(j, "snowflake_node_id", config.snowflakeNodeId);
    config.snowflakeEpochMs=ConfigParseHelper::getOrDefault(j, "snowflake_epoch_ms", config.snowflakeEpochMs);
    return config;
}
void IdConfig::applyEnvOverrides() {
    auto envNodeId=ConfigParseHelper::getEnv("SNOWFLAKE_NODE_ID");
    if(envNodeId.has_value()){
        snowflakeNodeId=ConfigParseHelper::parseEnvUInt(envNodeId.value(), "SNOWFLAKE_NODE_ID", 1023);
    }
    auto envEpochMs=ConfigParseHelper::getEnv("SNOWFLAKE_EPOCH_MS");
    if(envEpochMs.has_value()){
        snowflakeEpochMs=ConfigParseHelper::parseEnvUInt64(envEpochMs.value(), "SNOWFLAKE_EPOCH_MS", UINT64_MAX);
    }
}
void IdConfig::validateOrThrow() const {
    ConfigParseHelper::checkRange("snowflake_node_id", snowflakeNodeId, 0, 1023);
    uint64_t nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(snowflakeEpochMs>=nowMs){
        throw std::runtime_error("snowflake_epoch_ms must be less than current time");
    }
}