#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlErrorMapper.h"
#include "storage/sql/SqlTransaction.h"
#include <algorithm>
#include "third_party/json.hpp"
namespace {
    std::vector<std::uint64_t> normalizeIds(std::vector<std::uint64_t> ids){
    std::sort(ids.begin(),ids.end());
    ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    return ids;
}

std::string encodeIdsAsJson(const std::vector<std::uint64_t>& ids){
    nlohmann::json json(ids);
    return json.dump();
}

}
namespace storage{
SqlOfflineMessageRepo::SqlOfflineMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
RepoResult SqlOfflineMessageRepo::saveOfflineMessage(const std::string& accountId,uint64_t msgId,const std::string& groupId){
    if(accountId.empty()||groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="Invalid argument"};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(account_id,msg_id,group_id) VALUES(?,?,?)",{accountId,msgId,groupId});
        if(!result.ok()){
            auto status=mapSqlErrorToRepoStatus(result);
            if(status==RepoStatus::AlreadyExists)
                //重复保存离线索引作为幂等处理
                return RepoResult{.status=RepoStatus::Ok,.message="Offline message already exists"};
             return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result)};
        }
    }
    return RepoResult{.status=RepoStatus::Ok,.message="Offline message saved successfully"};
}
std::vector<OfflineMessageIndex> SqlOfflineMessageRepo::listOfflineMessage(const std::string& accountId,size_t limit){
    if(accountId.empty()){
        return {};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT msg_id,group_id,msg_type,peer_account_id FROM offline_messages WHERE account_id=? ORDER BY id ASC LIMIT ?",{accountId,limit});
        if(result.ok()){
            std::vector<OfflineMessageIndex> offlineMessages;
            for(auto& row:result.rows){
                OfflineMessageIndex offlineMessage;
                auto msgIdPair=row.find("msg_id");
                offlineMessage.msgId=msgIdPair!=row.end()?std::stoull(msgIdPair->second):0;
                auto msgTypePair=row.find("msg_type");
                if(msgTypePair!=row.end()){
                    if(std::stoi(msgTypePair->second)==1){
                        offlineMessage.type=OfflineMessageType::Group;
                    }
                    else{
                        offlineMessage.type=OfflineMessageType::Direct;
                    }
                }
                auto peerAccountIdPair=row.find("peer_account_id");
                if(peerAccountIdPair!=row.end()){
                    offlineMessage.peerAccountId=peerAccountIdPair->second;
                }
                auto groupIdPair=row.find("group_id");
                offlineMessage.groupId=groupIdPair!=row.end()?groupIdPair->second:"";

                offlineMessages.emplace_back(std::move(offlineMessage));

            }
            return offlineMessages;
        }
    }
    return {};
}

RepoValueResult<size_t> SqlOfflineMessageRepo::ackOfflineMessagesBatch(const std::string& accountId,const std::vector<uint64_t>& msgIds){
    if(accountId.empty()){
        return {.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(msgIds.empty()){//幂等
        return {.status=RepoStatus::Ok,.value=0};
    }
    //排序去重
    auto clearIds=normalizeIds(msgIds);
    auto idsJson=encodeIdsAsJson(clearIds);
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }

    auto result=conn->executePrepared("offline_message.delete_batch",
    R"(
        DELETE offline
        FROM offline_messages AS offline
        JOIN JSON_TABLE(
            CAST(? AS JSON),
            '$[*]' COLUMNS(
                msg_id BIGINT UNSIGNED PATH '$'
            )
        ) AS ids
        ON ids.msg_id = offline.msg_id
        WHERE offline.account_id = ?;
    )",{idsJson,accountId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.affectedRows==0){
        return {.status=RepoStatus::NotFound,.message=result.error};
    }
    return {.status=RepoStatus::Ok,.value=result.affectedRows};
    

}

RepoResult SqlOfflineMessageRepo::saveOfflineDirectMessage(const std::string& accountId,uint64_t msgId,const std::string& peerAccountId){
    if(accountId.empty()||peerAccountId.empty()||msgId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(account_id,msg_type,msg_id,peer_account_id,group_id) VALUES(?,2,?,?,NULL)",{accountId,msgId,peerAccountId});
        if(!result.ok()){
            //重复保存离线索引作为幂等处理
            auto status=mapSqlErrorToRepoStatus(result);
            if(status==RepoStatus::AlreadyExists){
                return RepoResult{.status=RepoStatus::Ok,.message="Offline message already exists"};
            }
            return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result)};
        }
    }
    return RepoResult{.status=RepoStatus::Ok,.message="Offline message saved successfully"};
}

RepoValueResult<size_t> SqlOfflineMessageRepo::deleteCreatedBefore(int64_t cutoffMs, size_t limit){
    if(cutoffMs<0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }

    auto result=conn->executePrepared(R"(
        DELETE FROM offline_messages
        WHERE created_at < FROM_UNIXTIME(? / 1000)
        LIMIT ?
        )",{cutoffMs,limit});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    
    return {.status=RepoStatus::Ok,.value=result.affectedRows};
}
}