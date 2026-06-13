#pragma once
#include <string>
#include <cstdint>

namespace storage{

enum class ConversationType:uint8_t{//区分会话类型
    Unknown=0,
    Direct=1,
    Group=2
};
struct ConversationSummary{
    std::string ownerAccountId{};//会话属于哪个账号
    ConversationType type{ConversationType::Unknown};//会话类型
    std::string targetId{};//对方id，accountId/groupId
    uint64_t lastMsgId{0};//会话最后一条消息ID
    std::string lastPreview{};//最后一条消息预览
    std::string lastSenderAccountId{};//最后一条消息发送者
    std::string lastSenderUsername{};//最后一条消息发送者用户名或昵称
    uint64_t lastTsMs{0};//最后一条消息时间
    uint32_t unreadCount{0};//当前未读数
    uint64_t lastReadMsgId{0};//当前账号已读到的最大消息ID
    uint64_t lastReadAtMs{0};//最近一次已读时间
};

inline std::string conversationTypeToString(ConversationType type){
    switch(type){
        case ConversationType::Group:
            return "group";
        case ConversationType::Direct:
            return "direct";
        default:
            return "unknown";
    }
}
}