#include "storage/sql/SqlFriendRequestRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
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
        if(result.error.find("Duplicate entry")!=std::string::npos){
            return {.status=RepoStatus::AlreadyExists};
        }
        else{
            return {.status=RepoStatus::SqlError,.message=result.error};
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
        if(result.rows.empty()){
            return {.status=RepoStatus::NotFound};
        }
        return {.status=RepoStatus::SqlError,.message=result.error};
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
            if(handledAtIt->second.empty()){
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

storage::RepoValueResult<storage::FriendRequest> storage::SqlFriendRequestRepo::findById(uint64_t requestId){
    if(requestId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    const auto& result=conn->queryPrepared("SELECT request_id,requester_account_id,receiver_account_id,status,created_at_ms,handled_at_ms  FROM friend_requests WHERE request_id=? AND status=0 LIMIT 1",{requestId});
    if(!result.ok()){
        if(result.rows.empty()){
            return {.status=RepoStatus::NotFound};
        }
        return {.status=RepoStatus::SqlError,.message=result.error};
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
        if(handledAtIt->second.empty()){
            friendRequest.handledAtMs=std::nullopt;
        }
        else{
            friendRequest.handledAtMs=std::stoll(handledAtIt->second);
        }
    }
    return {.status=RepoStatus::Ok,.value=friendRequest};

}
storage::RepoResult storage::SqlFriendRequestRepo::rejectPending(uint64_t requestId,const std::string&receiver,int64_t nowMs){
    if(requestId==0||receiver.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    //只允许拒绝状态为Pending的申请
    auto result=conn->executePrepared("UPDATE friend_requests SET status=2,handled_at_ms=? WHERE request_id=? AND receiver_account_id=? AND status=0",{nowMs,requestId,receiver});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
        
    }
    if(result.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message="Failed to update"};
        }
    return RepoResult{.status=RepoStatus::Ok};
}
storage::RepoResult storage::SqlFriendRequestRepo::acceptPendingAndCreateFriendPair(uint64_t requestId,const std::string& receiver,int64_t nowMs){
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
        auto requestResult=conn->queryPrepared("SELECT requester_account_id,receiver_account_id,status FROM friend_requests WHERE request_id=? FOR UPDATE",{requestId});
        if(!requestResult.ok()){
            return {.status=RepoStatus::SqlError,.message=requestResult.error};
        }
        if(requestResult.rows.empty()){
            return {.status=RepoStatus::NotFound};
            }
        //校验操作匹配且状态为pending
        std::string requestAccountId;
        std::string receiveAccountId;
        FriendRequestStatus status{FriendRequestStatus::Pending};
        const auto& row=requestResult.rows.front();
        auto requestPair=row.find("requester_account_id");
        if(requestPair!=row.end()){
            requestAccountId=requestPair->second;
        }
        if(requestAccountId.empty()){
            return {.status=RepoStatus::NotFound,.message="requestAccountId is empty"};
        }
        auto receivePair=row.find("receiver_account_id");
        if(receivePair!=row.end()){
            receiveAccountId=receivePair->second;
        }
        auto statusPair=row.find("status");
        if(statusPair!=row.end()){
            status=friendRequestStatusFromInt(std::stoi(statusPair->second));
        }
        if(receiver!=receiveAccountId||status!=FriendRequestStatus::Pending){
            return {.status=RepoStatus::NotFound};
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
        auto insertResult1=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,status,created_at_ms) VALUES(?,?,1,?) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{requestAccountId,receiveAccountId,nowMs,nowMs});
        if(!insertResult1.ok()){
            if(insertResult1.error.find("Duplicate entry")!=std::string::npos){
                return RepoResult{.status=RepoStatus::AlreadyExists,.message="already exists"};
            }
            return RepoResult{.status=RepoStatus::SqlError,.message=insertResult1.error};
        }
        auto insertResult2=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,status,created_at_ms) VALUES(?,?,1,?) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{receiveAccountId,requestAccountId,nowMs,nowMs});
        if(!insertResult2.ok()){
            if(insertResult2.error.find("Duplicate entry")!=std::string::npos){
                return RepoResult{.status=RepoStatus::AlreadyExists,.message="already exists"};
            }
            return RepoResult{.status=RepoStatus::SqlError,.message=insertResult2.error};
        }
        //全部成功后提交事务
        transaction.commit();
        return RepoResult{.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }
}