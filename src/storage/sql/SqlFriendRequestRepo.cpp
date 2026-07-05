#include "storage/sql/SqlFriendRequestRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
#include "storage/sql/SqlErrorMapper.h"
storage::SqlFriendRequestRepo::SqlFriendRequestRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}


storage::RepoValueResult<uint64_t> storage::SqlFriendRequestRepo::createPendingRequest(const std::string& requester,const std::string& receiver,int64_t nowMs){
    if(requester.empty()||receiver.empty()){
        return {.status=RepoStatus::InvalidArgument,.message="Requester or Receiver is empty"};
    }
    //禁止自己申请自己
    if(requester==receiver){
        return {.status=RepoStatus::CannotAddYourself,.message="Can not join yourself"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    auto result=conn->executePreParedInsert("INSERT INTO friend_requests(requester_account_id,receiver_account_id,status,created_at_ms) VALUES(?,?,0,?)",{requester,receiver,nowMs});
    if(!result.ok()){
        auto status=mapSqlErrorToRepoStatus(result);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="Request already exiest"};
        }
        else{
            return {.status=RepoStatus::SqlError,.message=formatSqlError(result)};
        }
    }
    return {.status=RepoStatus::Ok,.value=result.lastInsertId};

}

storage::RepoValueResult<std::vector<storage::FriendRequest>>  storage::SqlFriendRequestRepo::listPendingIncoming(const std::string&receiver){
    if(receiver.empty()){
        return {.status=RepoStatus::InvalidArgument,.message="Receiver accountId is empty"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    const auto& result=conn->queryPrepared("SELECT request_id,requester_account_id,receiver_account_id,status,created_at_ms,handled_at_ms  FROM friend_requests WHERE receiver_account_id=? AND status=0 ORDER BY request_id ASC",{receiver});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.rows.empty()){
            return {.status=RepoStatus::NotFound};
        }
    std::vector<FriendRequest> friendRequests;
    for(const auto& row:result.rows){
        FriendRequest friendRequest;
        auto requestIdIt=row.find("request_id");
        if(requestIdIt!=row.end()){
            friendRequest.requestId=std::stoull(requestIdIt->second);
        }
        auto requesterIt=row.find("requester_account_id");
        if(requesterIt!=row.end()){
            friendRequest.requestAccountId=requesterIt->second;
        }
        auto receiverIt=row.find("receiver_account_id");
        if(receiverIt!=row.end()){
            friendRequest.receiveAccountId=receiverIt->second;
        }
        auto statusIt=row.find("status");
        if(statusIt!=row.end()){
            friendRequest.status=friendRequestStatusFromInt(std::stoi(statusIt->second));
        }
        auto createdAtIt=row.find("created_at_ms");
        if(createdAtIt!=row.end()){
            friendRequest.createdAtMs=std::stoll(createdAtIt->second);
        }
        auto handledAtIt=row.find("handled_at_ms");
        if(handledAtIt!=row.end()){
            if(handledAtIt->second.empty()||handledAtIt->second=="NULL"){
                friendRequest.handledAtMs=std::nullopt;
            }
            else{
                friendRequest.handledAtMs=std::stoll(handledAtIt->second);
            }
        }
        friendRequests.emplace_back(std::move(friendRequest));
    }
    return {.status=RepoStatus::Ok,.value=friendRequests};
}

storage::RepoValueResult<storage::FriendRequest> storage::SqlFriendRequestRepo::findById(SqlConnection& conn, uint64_t requestId){
    if(requestId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    if(!conn.connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    const auto& result=conn.queryPrepared("SELECT request_id,requester_account_id,receiver_account_id,status,created_at_ms,handled_at_ms  FROM friend_requests WHERE request_id=? LIMIT 1 FOR UPDATE",{requestId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.rows.empty()){
        return {.status=RepoStatus::NotFound};
    }
    const auto& row=result.rows.front();
    FriendRequest friendRequest;
    auto requestIdIt=row.find("request_id");
    if(requestIdIt!=row.end()){
        friendRequest.requestId=std::stoull(requestIdIt->second);
    }
    auto requesterIt=row.find("requester_account_id");
    if(requesterIt!=row.end()){
        friendRequest.requestAccountId=requesterIt->second;
    }
    auto receiverIt=row.find("receiver_account_id");
    if(receiverIt!=row.end()){
        friendRequest.receiveAccountId=receiverIt->second;
    }
    auto statusIt=row.find("status");
    if(statusIt!=row.end()){
        friendRequest.status=friendRequestStatusFromInt(std::stoi(statusIt->second));
    }
    auto createdAtIt=row.find("created_at_ms");
    if(createdAtIt!=row.end()){
        friendRequest.createdAtMs=std::stoll(createdAtIt->second);
    }
    auto handledAtIt=row.find("handled_at_ms");
    if(handledAtIt!=row.end()){
        if(handledAtIt->second.empty()||handledAtIt->second=="NULL"){
            friendRequest.handledAtMs=std::nullopt;
        }
        else{
            friendRequest.handledAtMs=std::stoll(handledAtIt->second);
        }
    }
    return {.status=RepoStatus::Ok,.value=friendRequest};

}
storage::RepoValueResult<storage::FriendRequest> storage::SqlFriendRequestRepo::rejectPending(uint64_t requestId,const std::string&receiver,int64_t nowMs){
    if(requestId==0||receiver.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    try{
        //开启事务
        SqlTransaction transaction(*conn);
        //查询并校验申请存在
        auto requestResult=findById(*conn, requestId);
        if(!requestResult.ok()||!requestResult.value){
            return requestResult;
        }
        const auto& friendRequestValue=requestResult.value.value();
        FriendRequest friendRequest;
        friendRequest.requestId=friendRequestValue.requestId;
        friendRequest.requestAccountId=friendRequestValue.requestAccountId;
        friendRequest.receiveAccountId=friendRequestValue.receiveAccountId;
        friendRequest.status=friendRequestValue.status;
        //只允许拒绝状态为Pending的申请
        if(friendRequest.status!=FriendRequestStatus::Pending){
            return {.status=RepoStatus::AlreadyHandled};
        }
        //校验操作匹配
        if(friendRequest.receiveAccountId!=receiver){
            return {.status=RepoStatus::Forbidden,.message="You have no right to handle this request"};
        }
        //只允许拒绝状态为Pending的申请
        auto result=conn->executePrepared("UPDATE friend_requests SET status=2,handled_at_ms=? WHERE request_id=? AND receiver_account_id=? AND status=0",{nowMs,requestId,receiver});
        if(!result.ok()){//数据库异常
            return {.status=RepoStatus::SqlError,.message=result.error};
        
        }
        //找不到申请
        if(result.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message="Failed to update"};
         }
        //全部成功后提交事务
        transaction.commit();
        friendRequest.status=FriendRequestStatus::Rejected;
        friendRequest.handledAtMs=nowMs;
        return {.status=RepoStatus::Ok,.value=friendRequest};
    }catch(const std::exception& e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
}
storage::RepoValueResult<storage::FriendRequest> storage::SqlFriendRequestRepo::acceptPendingAndCreateFriendPair(uint64_t requestId,const std::string& receiver,int64_t nowMs){
    if(requestId==0||receiver.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    
    try{
        //开启事务
        SqlTransaction transaction(*conn);
        //查询并校验申请存在
        auto requestResult=findById(*conn, requestId);
        if(!requestResult.ok()||!requestResult.value){
            return requestResult;
        }
        const auto& friendRequestValue=requestResult.value.value();
        FriendRequest friendRequest;
        friendRequest.requestId=friendRequestValue.requestId;
        friendRequest.requestAccountId=friendRequestValue.requestAccountId;
        friendRequest.receiveAccountId=friendRequestValue.receiveAccountId;
        friendRequest.status=friendRequestValue.status;
        friendRequest.createdAtMs=friendRequestValue.createdAtMs;
        if(friendRequest.status!=FriendRequestStatus::Pending){
            return {.status=RepoStatus::AlreadyHandled};
        }
        //校验操作匹配且状态为pending
        if(friendRequest.receiveAccountId!=receiver){
            return {.status=RepoStatus::Forbidden,.message="You have no right to handle this request"};
        }
        
        //执行同意申请
        auto acceptResult=conn->executePrepared("UPDATE friend_requests SET status=1,handled_at_ms=? WHERE request_id=? AND status=0",{nowMs,requestId});
        if(!acceptResult.ok()){
            return {.status=RepoStatus::SqlError,.message=acceptResult.error};
        }
        if(acceptResult.affectedRows==0){
                return {.status=RepoStatus::NotFound,.message="Failed to update"};
            }
        //执行双向添加好友
        auto insertResult1=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,status,created_at_ms) VALUES(?,?,1,?) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{friendRequest.requestAccountId,friendRequest.receiveAccountId,nowMs,nowMs});
        if(!insertResult1.ok()){
           auto status=mapSqlErrorToRepoStatus(insertResult1);
            if(status==RepoStatus::AlreadyExists){
                return {.status=RepoStatus::AlreadyExists,.message="request already exiest"};
            }
            return {.status=RepoStatus::SqlError,.message=formatSqlError(insertResult1)};
        }
        auto insertResult2=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,status,created_at_ms) VALUES(?,?,1,?) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{friendRequest.receiveAccountId,friendRequest.requestAccountId,nowMs,nowMs});
        if(!insertResult2.ok()){
            auto status=mapSqlErrorToRepoStatus(insertResult2);
            if(status==RepoStatus::AlreadyExists){
                return {.status=RepoStatus::AlreadyExists,.message="request already exiest"};
            }
            return {.status=RepoStatus::SqlError,.message=formatSqlError(insertResult2)};
        }
        //全部成功后提交事务
        transaction.commit();
        friendRequest.status=FriendRequestStatus::Accepted;
        friendRequest.handledAtMs=nowMs;
        return {.status=RepoStatus::Ok,.value=friendRequest};
    }catch(const std::exception& e){
        return {.status=RepoStatus::SqlError,.message=e.what()};
    }
}