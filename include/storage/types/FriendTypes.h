#pragma once
#include <string>
#include <cstdint>
#include <optional>
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


enum class FriendRequestStatus{//好友申请状态枚举
    Pending,//等待处理
    Accepted,//已同意
    Rejected,//已拒绝
    Cancelled//取消申请
};

struct FriendRequest{
    uint64_t requestId{0};//申请唯一编号
    std::string requestAccountId{};//发起人账号
    std::string receiveAccountId{};//接收人账号
    FriendRequestStatus status{FriendRequestStatus::Pending};//当前状态
    int64_t createdAtMs{};//创建时间
    std::optional<int64_t> handledAtMs{};//处理时间
};

inline FriendRequestStatus friendRequestStatusFromInt(int v){
    switch(v){
        case 0:
            return FriendRequestStatus::Pending;
        case 1:
            return FriendRequestStatus::Accepted;
        case 2:
            return FriendRequestStatus::Rejected;
        case 3:
            return FriendRequestStatus::Cancelled;
        default:
            return FriendRequestStatus::Pending;//默认返回待处理
    }
}
}