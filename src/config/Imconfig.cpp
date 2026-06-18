#include "config/ImConfig.h"
ImConfig ImConfig::fromJson(const nlohmann::json& j){
    ImConfig imConfig;
    imConfig.requireGroupIdForSend=ConfigParseHelper::getOrDefault(j,"require_group_id_for_send",imConfig.requireGroupIdForSend);
    imConfig.maxGroupNameLen=ConfigParseHelper::getOrDefault(j,"max_group_name_len",imConfig.maxGroupNameLen);
    imConfig.maxMessageLen=ConfigParseHelper::getOrDefault(j,"max_message_len",imConfig.maxMessageLen);
    imConfig.allowDebugAuth=ConfigParseHelper::getOrDefault(j,"allow_debug_auth",imConfig.allowDebugAuth);
    imConfig.maxAckBatchSize=ConfigParseHelper::getOrDefault(j,"max_ack_batch_size",imConfig.maxAckBatchSize);
    imConfig.maxGroupMembers=ConfigParseHelper::getOrDefault(j,"max_group_members",imConfig.maxGroupMembers);
    imConfig.requireFriendForGroupInvite=ConfigParseHelper::getOrDefault(j,"require_friend_for_group_invite",imConfig.requireFriendForGroupInvite);
    imConfig.defaultHistoryLimit=ConfigParseHelper::getOrDefault(j,"default_history_limit",imConfig.defaultHistoryLimit);   
    imConfig.maxHistoryLimit=ConfigParseHelper::getOrDefault(j,"max_history_limit",imConfig.maxHistoryLimit);
    imConfig.maxSyncCursorCount=ConfigParseHelper::getOrDefault(j,"max_sync_cursor_count",imConfig.maxSyncCursorCount);
    imConfig.maxSyncMessageLimit=ConfigParseHelper::getOrDefault(j,"max_sync_message_limit",imConfig.maxSyncMessageLimit);
    imConfig.maxOfflineIndexLimit=ConfigParseHelper::getOrDefault(j,"max_offline_index_limit",imConfig.maxOfflineIndexLimit);
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
    auto envMaxGroupMembers=ConfigParseHelper::getEnv("IM_MAX_GROUP_MEMBERS");
    if(envMaxGroupMembers.has_value()){
        maxGroupMembers=ConfigParseHelper::parseEnvUInt(envMaxGroupMembers.value(), "IM_MAX_GROUP_MEMBERS", 1000);
    }
    auto envRequireFriendForGroupInvite=ConfigParseHelper::getEnv("IM_REQUIRE_FRIEND_FOR_GROUP_INVITE");
    if(envRequireFriendForGroupInvite.has_value()){
        requireFriendForGroupInvite=ConfigParseHelper::parseEnvBool(envRequireFriendForGroupInvite.value(), "IM_REQUIRE_FRIEND_FOR_GROUP_INVITE");
    }
    auto envMaxAckSize=ConfigParseHelper::getEnv("IM_MAX_ACK_BATCH_SIZE");
    if(envMaxAckSize.has_value()){
        maxAckBatchSize=ConfigParseHelper::parseEnvUInt(envMaxAckSize.value(), "IM_MAX_ACK_BATCH_SIZE", 1024);
    }
    auto envMaxHistoryLimit=ConfigParseHelper::getEnv("IM_MAX_HISTORY_LIMIT");
    if(envMaxHistoryLimit.has_value()){
        maxHistoryLimit=ConfigParseHelper::parseEnvUInt(envMaxHistoryLimit.value(), "IM_MAX_HISTORY_LIMIT", 1000);
    }
    auto envMaxSyncCursorCount=ConfigParseHelper::getEnv("IM_MAX_SYNC_CURSOR_COUNT");
    if(envMaxSyncCursorCount.has_value()){
        maxSyncCursorCount=ConfigParseHelper::parseEnvUInt(envMaxSyncCursorCount.value(), "IM_MAX_SYNC_CURSOR_COUNT", 1000);
    }
    auto envMaxSyncMessageLimit=ConfigParseHelper::getEnv("IM_MAX_SYNC_MESSAGE_LIMIT");
    if(envMaxSyncMessageLimit.has_value()){
        maxSyncMessageLimit=ConfigParseHelper::parseEnvUInt(envMaxSyncMessageLimit.value(), "IM_MAX_SYNC_MESSAGE_LIMIT", 1000);
    }
    auto envMaxOfflineIndexLimit=ConfigParseHelper::getEnv("IM_MAX_OFFLINE_INDEX_LIMIT");
    if(envMaxOfflineIndexLimit.has_value()){
        maxOfflineIndexLimit=ConfigParseHelper::parseEnvUInt(envMaxOfflineIndexLimit.value(), "IM_MAX_OFFLINE_INDEX_LIMIT", 1000);
    }
}
void ImConfig::validateOrThrow() const{
    ConfigParseHelper::checkRange("maxGroupNameLen", maxGroupNameLen, 1, 128);
    ConfigParseHelper::checkRange("maxMessageLen", maxMessageLen, 1, 1024*1024);
    ConfigParseHelper::checkRange("maxAckBatchSize",maxAckBatchSize,1,1000);
    ConfigParseHelper::checkRange("maxGroupMembers", maxGroupMembers, 1, 1000);
    ConfigParseHelper::checkRange("requireFriendForGroupInvite", requireFriendForGroupInvite, 0, 1);
    ConfigParseHelper::checkRange("defaultHistoryLimit", defaultHistoryLimit, 1, maxHistoryLimit);
    ConfigParseHelper::checkRange("maxHistoryLimit", maxHistoryLimit, 1, 500);
    ConfigParseHelper::checkRange("maxSyncCursorCount", maxSyncCursorCount, 1, 500);
    ConfigParseHelper::checkRange("maxSyncMessageLimit", maxSyncMessageLimit, 1, 500);
    ConfigParseHelper::checkRange("maxOfflineIndexLimit", maxOfflineIndexLimit, 1, 500);
    ConfigParseHelper::checkRange("maxSyncMessageLimit", maxSyncMessageLimit, 1, 500);  
    ConfigParseHelper::checkRange("maxOfflineIndexLimit", maxOfflineIndexLimit, 1, 500);
}
