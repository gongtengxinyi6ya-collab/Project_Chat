#pragma once
#include <cstddef>
#include "third_party/json.hpp"
#include "ConfigParseHelper.h"
/*协议约束参数化，避免ImService里写死常量*/

class ImConfig{
public:
    static ImConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;
    
    //属性
    bool requireGroupIdForSend{true};
    size_t maxGroupNameLen{64};//群名称长度
    size_t maxMessageLen{4096};//消息最长长度
    bool allowDebugAuth{false};
    size_t maxAckBatchSize{200};//限制单次ack的最大消息数量
    size_t maxGroupMembers{200};//限制单群成员数
    bool requireFriendForGroupInvite{true};//控制邀请者和被邀请者是否必须是好友
    
    size_t defaultHistoryLimit{50};
    size_t maxHistoryLimit{100};
    size_t maxSyncCursorCount{50};
    size_t maxSyncMessageLimit{100};
    size_t maxOfflineIndexLimit{200};
};