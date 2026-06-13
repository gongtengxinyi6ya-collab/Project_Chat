#pragma once
#include <string>
#include <cstdint>
#include <optional>
namespace storage{
enum class FriendRelationStatus:uint8_t{//好友关系状态
    Active=1,
    Deleted=2

};

struct FriendRelation{
    std::string accountId{};//关系所属账号
    std::string friendAccountId{};//好友账号
    int64_t createdAtMs{0};//添加好友时间
    FriendRelationStatus status{FriendRelationStatus::Active};

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