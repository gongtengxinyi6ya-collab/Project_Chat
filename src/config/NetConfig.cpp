#include "config/NetConfig.h"

NetConfig NetConfig::fromJson(const nlohmann::json& j){
    NetConfig netConfig;
    netConfig.heartBeatMs=ConfigParseHelper::getOrDefault(j,"heartbeat_ms",netConfig.heartBeatMs);
    netConfig.heartbeatTimeoutMs=ConfigParseHelper::getOrDefault(j,"heartbeat_timeout_ms",netConfig.heartbeatTimeoutMs);
    netConfig.idleTimeoutMs=ConfigParseHelper::getOrDefault(j,"idle_timeout_ms",netConfig.idleTimeoutMs);
    netConfig.maxFrameLen=ConfigParseHelper::getOrDefault(j,"max_frame_len",netConfig.maxFrameLen);
    netConfig.connHighWaterMark=ConfigParseHelper::getOrDefault(j,"conn_high_water_mark",netConfig.connHighWaterMark);
    netConfig.connLowWaterMark=ConfigParseHelper::getOrDefault(j,"conn_low_water_mark",netConfig.connLowWaterMark);
    netConfig.connHardLimit=ConfigParseHelper::getOrDefault(j,"conn_hard_limit",netConfig.connHardLimit);
    return netConfig;
}

void NetConfig::applyEnvOverrides(){
    auto envHeartBeatMs=ConfigParseHelper::getEnv("CHAT_HEARTBEAT_MS");
    if(envHeartBeatMs.has_value()){
        heartBeatMs=ConfigParseHelper::parseEnvUInt(envHeartBeatMs.value(),"CHAT_HEARTBEAT_MS");
    }
    auto envHeartbeatTimeoutMs=ConfigParseHelper::getEnv("CHAT_HEARTBEAT_TIMEOUT_MS");
    if(envHeartbeatTimeoutMs.has_value()){
        heartbeatTimeoutMs=ConfigParseHelper::parseEnvUInt(envHeartbeatTimeoutMs.value(),"CHAT_HEARTBEAT_TIMEOUT_MS");
    }
    auto envIdleTimeoutMs=ConfigParseHelper::getEnv("CHAT_IDLE_TIMEOUT_MS");
    if(envIdleTimeoutMs.has_value()){
        idleTimeoutMs=ConfigParseHelper::parseEnvUInt(envIdleTimeoutMs.value(),"CHAT_IDLE_TIMEOUT_MS");
    }
    auto envMaxFrameLen=ConfigParseHelper::getEnv("CHAT_MAX_FRAME_LEN");
    if(envMaxFrameLen.has_value()){
        maxFrameLen=ConfigParseHelper::parseEnvUInt(envMaxFrameLen.value(),"CHAT_MAX_FRAME_LEN");
    }
    auto envConnHighWaterMark=ConfigParseHelper::getEnv("CHAT_CONN_HIGH_WATER_MARK");
    if(envConnHighWaterMark.has_value()){
        connHighWaterMark=ConfigParseHelper::parseEnvUInt(envConnHighWaterMark.value(),"CHAT_CONN_HIGH_WATER_MARK");
    }
    auto envConnLowWaterMark=ConfigParseHelper::getEnv("CHAT_CONN_LOW_WATER_MARK");
    if(envConnLowWaterMark.has_value()){
        connLowWaterMark=ConfigParseHelper::parseEnvUInt(envConnLowWaterMark.value(),"CHAT_CONN_LOW_WATER_MARK");
    }
    auto envConnHardLimit=ConfigParseHelper::getEnv("CHAT_CONN_HARD_LIMIT");
    if(envConnHardLimit.has_value()){
        connHardLimit=ConfigParseHelper::parseEnvUInt(envConnHardLimit.value(),"CHAT_CONN_HARD_LIMIT");
    }
}
void NetConfig::validateOrThrow() const{
    ConfigParseHelper::checkRange("heartbeat_ms", heartBeatMs, 1000, 300000);
    ConfigParseHelper::checkRange("heartbeat_timeout_ms", heartbeatTimeoutMs, heartBeatMs, 300000);
    ConfigParseHelper::checkRange("idle_timeout_ms", idleTimeoutMs, heartBeatMs, 300000);
    ConfigParseHelper::checkRange("max_frame_len", maxFrameLen, 1024, 10*1024*1024);
    ConfigParseHelper::checkRange("conn_high_water_mark", connHighWaterMark, 1024, 100*1024*1024);
    ConfigParseHelper::checkRange("conn_low_water_mark", connLowWaterMark, 512, connHighWaterMark);
    ConfigParseHelper::checkRange("conn_hard_limit", connHardLimit, connHighWaterMark, 100*1024*1024);
}