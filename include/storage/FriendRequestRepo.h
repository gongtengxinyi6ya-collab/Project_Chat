#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include "storage/RepoResult.h"
#include "storage/RepoValueResult.h"
#include "storage/types/FriendTypes.h"
/*
好友申请存储抽象：提供创建申请，同意，拒绝申请等接口*/
namespace storage{
class SqlConnection;

class FriendRequestRepo{
public:
    virtual ~FriendRequestRepo()=default;
    virtual RepoValueResult<uint64_t> createPendingRequest(const std::string&requester,const std::string& receiver,int64_t nowMs)=0;//插入待处理申请
    virtual RepoValueResult<std::vector<FriendRequest>> listPendingIncoming(const std::string& receiver)=0;//查询接收人尚未处理的申请
    virtual RepoValueResult<FriendRequest> rejectPending(uint64_t requestId,const std::string& receiver,int64_t nowMs)=0;//拒绝状态为待处理的申请
    virtual RepoValueResult<FriendRequest> acceptPendingAndCreateFriendPair(uint64_t requestId,const std::string& receiver,int64_t nowMs)=0;//事务内同意申请并建立好友关系
    virtual RepoValueResult<size_t> deleteHandledBefore(int64_t cutoffMs, size_t limit) = 0;//删除已处理好友申请
};

}