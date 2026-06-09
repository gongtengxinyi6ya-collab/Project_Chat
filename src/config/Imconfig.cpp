#include "config/ImConfig.h"
ImConfig ImConfig::fromJson(const nlohmann::json& j){
    ImConfig imConfig;
    imConfig.requireGroupIdForSend=ConfigParseHelper::getOrDefault(j,"require_group_id_for_send",imConfig.requireGroupIdForSend);
    imConfig.maxGroupNameLen=ConfigParseHelper::getOrDefault(j,"max_group_name_len",imConfig.maxGroupNameLen);
    imConfig.maxMessageLen=ConfigParseHelper::getOrDefault(j,"max_message_len",imConfig.maxMessageLen);
    imConfig.allowDebugAuth=ConfigParseHelper::getOrDefault(j,"allow_debug_auth",imConfig.allowDebugAuth);
    imConfig.maxAckBatchSize=ConfigParseHelper::getOrDefault(j,"max_ack_batch_size",imConfig.maxAckBatchSize);
    return imConfig;
}
void ImConfig::applyEnvOverrides(){
    auto envRequireGroupIdForSend=ConfigParseHelper::getEnv("IM_REQUIRE_GROUP_ID_FOR_SEND");
    if(envRequireGroupIdForSend.has_value()){
        requireGroupIdForSend=ConfigParseHelper::parseEnvBool(envRequireGroupIdForSend.value(), "IM_REQUIRE_GROUP_ID_FOR_SEND");
    }
    auto envMaxGroupNameLen=ConfigParseHelper::getEnv("IM_MAX_GROUP_NAME_LEN");
    if(envMaxGroupNameLen.has_value()){
        maxGroupNameLen=ConfigParseHelper::parseEnvUInt(envMaxGroupNameLen.value(), "IM_MAX_GROUP_NAME_LEN", 1024);
    }
    auto envMaxMessageLen=ConfigParseHelper::getEnv("IM_MAX_MESSAGE_LEN");
    if(envMaxMessageLen.has_value()){
        maxMessageLen=ConfigParseHelper::parseEnvUInt(envMaxMessageLen.value(), "IM_MAX_MESSAGE_LEN", 1024*1024);
    }
    auto envAllowDebugAuth=ConfigParseHelper::getEnv("IM_ALLOW_DEBUG_AUTH");
    if(envAllowDebugAuth.has_value()){
        allowDebugAuth=ConfigParseHelper::parseEnvBool(envAllowDebugAuth.value(), "IM_ALLOW_DEBUG_AUTH");
    }
    auto envMaxAckSize=ConfigParseHelper::getEnv("IM_MAX_ACK_BATCH_SIZE");
    if(envMaxAckSize.has_value()){
        maxAckBatchSize=ConfigParseHelper::parseEnvUInt(envMaxAckSize.value(), "IM_MAX_ACK_BATCH_SIZE", 1024);
    }
}
void ImConfig::validateOrThrow() const{
    ConfigParseHelper::checkRange("maxGroupNameLen", maxGroupNameLen, 1, 128);
    ConfigParseHelper::checkRange("maxMessageLen", maxMessageLen, 1, 1024*1024);
    ConfigParseHelper::checkRange("maxAckBatchSize",maxAckBatchSize,1,1000);
}
