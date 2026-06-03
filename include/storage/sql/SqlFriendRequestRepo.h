#pragma once
#include <memory>
#include "storage/FriendRequestRepo.h"

/*实现SQL类*/
namespace storage{
    class SqlConnectionPool;
    class SqlConnection;

class SqlFriendRequestRepo:public FriendRequestRepo{
public:
    explicit SqlFriendRequestRepo(std::shared_ptr<SqlConnectionPool> pool);
    RepoValueResult<uint64_t> createPendingRequest(const std::string&requester,const std::string& receiver,int64_t nowMs)override;//插入待处理申请
    RepoValueResult<std::vector<FriendRequest>> listPendingIncoming(const std::string& receiver)override;//查询接收人尚未处理的申请
    RepoValueResult<FriendRequest> rejectPending(uint64_t requestId,const std::string& receiver,int64_t nowMs)override;//拒绝状态为待处理的申请
    RepoValueResult<FriendRequest> acceptPendingAndCreateFriendPair(uint64_t requestId,const std::string& receiver,int64_t nowMs)override;//事务内同意申请并建立好友关系
private:
    std::shared_ptr<SqlConnectionPool> pool_;//获取MySql连接
    RepoValueResult<FriendRequest> findById(SqlConnection& conn, uint64_t requestId);//查询指定申请
};
}