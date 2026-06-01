#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include "storage/RepoResult.h"
#include "storage/RepoValueResult.h"
/*
好友申请存储抽象：提供创建申请，同意，拒绝申请等接口*/
namespace storage{

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

class FriendRequestRepo{
    virtual ~FriendRequestRepo()=default;
    virtual RepoValueResult<uint64_t> createPendingRequest(const std::string&requester,const std::string receiver,int64_t nowMs)=0;//插入待处理申请
    virtual RepoValueResult<std::vector<FriendRequest>> listPendingIncoming(const std::string& receiver)=0;//查询接收人尚未处理的申请
    virtual RepoValueResult<FriendRequest> findById(uint64_t requestId)=0;//查询指定申请
    virtual RepoResult rejectPending(uint64_t requestId,const std::string& receiver,int64_t nowMs)=0;//拒绝状态为待处理的申请
    virtual RepoResult acceptPendingAndCreateFriendPair(uint64_t requestId,const std::string& receiver,int64_t nowMs)=0;//事务内同意申请并建立好友关系
};
}