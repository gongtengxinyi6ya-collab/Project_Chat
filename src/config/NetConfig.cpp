#include "config/NetConfig.h"

NetConfig NetConfig::fromJson(const nlohmann::json& j){
    NetConfig netConfig;
    netConfig.heartBeatMs=ConfigParseHelper::getOrDefault(j,"heartbeat_ms",netConfig.heartBeatMs);
    netConfig.idleTimeoutMs=ConfigParseHelper::getOrDefault(j,"idle_timeout_ms",netConfig.idleTimeoutMs);
    netConfig.maxFrameLen=ConfigParseHelper::getOrDefault(j,"max_frame_len",netConfig.maxFrameLen);
    return netConfig;
}

void NetConfig::applyEnvOverrides(){
    auto envHeartBeatMs=ConfigParseHelper::getEnv("CHAT_HEARTBEAT_MS");
    if(envHeartBeatMs.has_value()){
        heartBeatMs=ConfigParseHelper::parseEnvUInt(envHeartBeatMs.value(),"CHAT_HEARTBEAT_MS");
    }
    auto envIdleTimeoutMs=ConfigParseHelper::getEnv("CHAT_IDLE_TIMEOUT_MS");
    if(envIdleTimeoutMs.has_value()){
        idleTimeoutMs=ConfigParseHelper::parseEnvUInt(envIdleTimeoutMs.value(),"CHAT_IDLE_TIMEOUT_MS");
    }
    auto envMaxFrameLen=ConfigParseHelper::getEnv("CHAT_MAX_FRAME_LEN");
    if(envMaxFrameLen.has_value()){
        maxFrameLen=ConfigParseHelper::parseEnvUInt(envMaxFrameLen.value(),"CHAT_MAX_FRAME_LEN");
    }
}
void NetConfig::validateOrThrow() const{
    ConfigParseHelper::checkRange("heartbeat_ms", heartBeatMs, 1000, 300000);
    ConfigParseHelper::checkRange("idle_timeout_ms", idleTimeoutMs, heartBeatMs, 300000);
    ConfigParseHelper::checkRange("max_frame_len", maxFrameLen, 1024, 10*1024*1024);
}