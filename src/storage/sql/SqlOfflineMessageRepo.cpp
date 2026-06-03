#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
storage::SqlOfflineMessageRepo::SqlOfflineMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}

storage::RepoResult storage::SqlOfflineMessageRepo::saveOfflineMessage(const std::string& accountId,uint64_t msgId,const std::string& groupId){
    if(accountId.empty()||groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="Invalid argument"};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(account_id,msg_id,group_id) VALUES(?,?,?)",{accountId,msgId,groupId});
        if(!result.ok()&&result.error.find("Duplicate entry")!=std::string::npos){
            //重复保存离线索引作为幂等处理
            return RepoResult{.status=RepoStatus::Ok,.message="Offline message already exists"};
        }
        if(!result.ok()){
            return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
        }
    }
    return RepoResult{.status=RepoStatus::Ok,.message="Offline message saved successfully"};
}
std::vector<storage::OfflineMessageIndex> storage::SqlOfflineMessageRepo::listOfflineMessage(const std::string& accountId,size_t limit){
    if(accountId.empty()){
        return {};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT msg_id,group_id FROM offline_messages WHERE account_id=? ORDER BY id ASC LIMIT ?",{accountId,limit});
        if(result.ok()){
            std::vector<OfflineMessageIndex> offlineMessages;
            for(auto& row:result.rows){
                OfflineMessageIndex offlineMessage;
                auto msgIdPair=row.find("msg_id");
                offlineMessage.msgId=msgIdPair!=row.end()?std::stoull(msgIdPair->second):0;
                auto groupIdPair=row.find("group_id");
                offlineMessage.groupId=groupIdPair!=row.end()?groupIdPair->second:"";
                offlineMessages.emplace_back(std::move(offlineMessage));
            }
            return offlineMessages;
        }
    }
    return {};
}

storage::RepoResult storage::SqlOfflineMessageRepo::ackOfflineMessage(const std::string& accountId,const std::vector<uint64_t>& msgIds){
    if(accountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(msgIds.empty()){
        return RepoResult{.status=RepoStatus::Ok};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        //开启事务
        SqlTransaction transaction(*conn);
        for(auto msgId:msgIds){
            auto result=conn->executePrepared("DELETE FROM offline_messages WHERE account_id=? AND msg_id=?",{accountId,msgId});
            if(!result.ok()){
                return RepoResult{.status=RepoStatus::SqlError,.message="Failed to delete message"};
            }
        }
        transaction.commit();
        return RepoResult{.status=RepoStatus::Ok};
    }
    return RepoResult{.status=RepoStatus::SqlError};

}

storage::RepoResult storage::SqlOfflineMessageRepo::saveOfflineDirectMessage(const std::string& accountId,uint64_t msgId,const std::string& peerAccountId){
    if(accountId.empty()||peerAccountId.empty()||msgId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(account_id,msg_type,msg_id,peer_account_id,group_id) VALUES(?,2,?,?,NULL)",{accountId,msgId,peerAccountId});
        if(!result.ok()&&result.error.find("Duplicate entry")!=std::string::npos){
            //重复保存离线索引作为幂等处理
            return RepoResult{.status=RepoStatus::Ok,.message="Offline message already exists"};
        }
        if(!result.ok()){
            return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
        }
    }
    return RepoResult{.status=RepoStatus::Ok,.message="Offline message saved successfully"};
}