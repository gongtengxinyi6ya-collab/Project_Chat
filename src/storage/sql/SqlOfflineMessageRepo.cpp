#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
storage::SqlOfflineMessageRepo::SqlOfflineMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}

storage::RepoResult storage::SqlOfflineMessageRepo::saveOfflineMessage(const std::string& username,uint64_t msgId,const std::string& groupId){
    if(username.empty()||groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="Invalid argument"};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(username,msg_id,group_id) VALUES(?,?,?)",{username,msgId,groupId});
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
std::vector<storage::OfflineMessageIndex> storage::SqlOfflineMessageRepo::listOfflineMessage(const std::string& username,size_t limit){
    if(username.empty()){
        return {};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT msg_id,group_id FROM offline_messages WHERE username=? ORDER BY id ASC LIMIT ?",{username,limit});
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

storage::RepoResult storage::SqlOfflineMessageRepo::ackOfflineMessage(const std::string& username,const std::vector<uint64_t>& msgIds){
    if(username.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="username is empty"};
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
            auto result=conn->executePrepared("DELETE FROM offline_messages WHERE username=? AND msg_id=?",{username,msgId});
            if(!result.ok()){
                return RepoResult{.status=RepoStatus::SqlError,.message="Failed to delete message"};
            }
        }
        transaction.commit();
        return RepoResult{.status=RepoStatus::Ok};
    }
    return RepoResult{.status=RepoStatus::SqlError};

}