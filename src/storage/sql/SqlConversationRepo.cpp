#include "storage/sql/SqlConversationRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include <stdexcept>
storage::SqlConversationRepo::SqlConversationRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::RepoResult storage::SqlConversationRepo::upsertDirectOnMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs){
    if(senderAccountId.empty()||receiverAccountId.empty()||msgId==0||serverTsMs==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    std::string finalPreview=preview;
    if(finalPreview.size()>200){
        finalPreview=finalPreview.substr(0,200);
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the dataBase"};
    }
    try{
        //开启事务
        SqlTransaction transation(*conn);
        const std::string sql1=R"(
        INSERT INTO conversations 
        (owner_account_id,
        conversation_type,
        target_id,
        last_msg_id,
        last_preview,
        last_sender_account_id,
        last_sender_username,
        last_ts_ms,
        unread_count,
        last_read_msg_id,
        last_read_at_ms)
        VALUES (?, 1, ?, ?, ?, ?, ?, ?, 0, ?, ?)
        ON DUPLICATE KEY UPDATE
            last_msg_id = VALUES(last_msg_id),
            last_preview = VALUES(last_preview),
            last_sender_account_id = VALUES(last_sender_account_id),
            last_sender_username = VALUES(last_sender_username),
            last_ts_ms = VALUES(last_ts_ms),
            unread_count = 0,
            last_read_msg_id = VALUES(last_read_msg_id),
            last_read_at_ms = VALUES(last_read_at_ms))";
        auto result1=conn->executePrepared(sql1,{senderAccountId,receiverAccountId,msgId,finalPreview,senderAccountId,senderUsername,serverTsMs,msgId,serverTsMs});
        if(!result1.ok()){
            return {.status=RepoStatus::SqlError,.message=result1.error};
        }
        const std::string sql2=R"(
        INSERT INTO conversations (
            owner_account_id,
            conversation_type,
            target_id,
            last_msg_id,
            last_preview,
            last_sender_account_id,
            last_sender_username,
            last_ts_ms,
            unread_count
        )
        VALUES (?, 1, ?, ?, ?, ?, ?, ?, 1)
        ON DUPLICATE KEY UPDATE
            last_msg_id = VALUES(last_msg_id),
            last_preview = VALUES(last_preview),
            last_sender_account_id = VALUES(last_sender_account_id),
            last_sender_username = VALUES(last_sender_username),
            last_ts_ms = VALUES(last_ts_ms),
            unread_count = unread_count + 1
        )";
        auto result2=conn->executePrepared(sql2,{receiverAccountId,senderAccountId,msgId,finalPreview,senderAccountId,senderUsername,serverTsMs});
        if(!result2.ok()){
            return {.status=RepoStatus::SqlError,.message=result2.error};
        }
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception&e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
}

storage::RepoResult storage::SqlConversationRepo::upsertGroupOnMessage(const std::string&groupId,const std::vector<std::string>&memberAccountIds,const std::string& senderAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs){
    if(groupId.empty()||memberAccountIds.empty()||senderAccountId.empty()||msgId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    std::string finalPreview=preview;
    if(finalPreview.size()>200){
        finalPreview=finalPreview.substr(0,200);
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the dataBase"};
    }
    try{
        //开启事务
        SqlTransaction transation(*conn);
        //遍历所有群成员
        for(const auto& memberAccountId:memberAccountIds){
            if(memberAccountId==senderAccountId){
                const std::string sql=R"(
                INSERT INTO conversations 
                (owner_account_id,
                conversation_type,
                target_id,
                last_msg_id,
                last_preview,
                last_sender_account_id,
                last_sender_username,
                last_ts_ms,
                unread_count,
                last_read_msg_id,
                last_read_at_ms)
                VALUES (?, 2, ?, ?, ?, ?, ?, ?, 0, ?, ?)
                ON DUPLICATE KEY UPDATE
                    last_msg_id = VALUES(last_msg_id),
                    last_preview = VALUES(last_preview),
                    last_sender_account_id = VALUES(last_sender_account_id),
                    last_sender_username = VALUES(last_sender_username),
                    last_ts_ms = VALUES(last_ts_ms),
                    unread_count = 0,
                    last_read_msg_id = GREATEST(conversations.last_read_msg_id,VALUES(last_read_msg_id)),
                    last_read_at_ms = GREATEST(conversations.last_read_at_ms,VALUES(last_read_at_ms)))";
                auto result=conn->executePrepared(sql,{memberAccountId,groupId,msgId,finalPreview,senderAccountId,senderUsername,serverTsMs,msgId,serverTsMs});
                if(!result.ok()){
                    return {.status=RepoStatus::SqlError,.message=result.error};
                }
            }
            else{
                const std::string sql=R"(
                INSERT INTO conversations 
                (owner_account_id,
                conversation_type,
                target_id,
                last_msg_id,
                last_preview,
                last_sender_account_id,
                last_sender_username,
                last_ts_ms,
                unread_count,
                last_read_msg_id,
                last_read_at_ms)
                VALUES (?, 2, ?, ?, ?, ?, ?, ?, 1, ?, ?)
                ON DUPLICATE KEY UPDATE
                    last_msg_id = VALUES(last_msg_id),
                    last_preview = VALUES(last_preview),
                    last_sender_account_id = VALUES(last_sender_account_id),
                    last_sender_username = VALUES(last_sender_username),
                    last_ts_ms = VALUES(last_ts_ms),
                    unread_count = conversations.unread_count+VALUES(unread_count),
                    last_read_msg_id = GREATEST(conversations.last_read_msg_id,VALUES(last_read_msg_id)),
                    last_read_at_ms = GREATEST(conversations.last_read_at_ms,VALUES(last_read_at_ms)))";
                    auto result=conn->executePrepared(sql,{memberAccountId,groupId,msgId,finalPreview,senderAccountId,senderUsername,serverTsMs,msgId,serverTsMs});
                if(!result.ok()){
                    return {.status=RepoStatus::SqlError,.message=result.error};
                }
            }
    }
    //事务提交
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception&e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
}
std::vector<storage::ConversationSummary> storage::SqlConversationRepo::listConversations(const std::string& ownerAccountId,size_t limit){
    if(ownerAccountId.empty()){
        return {};
    }
    if(limit==0){
        limit=20;
    }
    if(limit>100){
        limit=100;
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    std::string sql=R"(
    SELECT 
        owner_account_id,
        conversation_type,
       target_id,
       last_msg_id,
       last_preview,
       last_sender_account_id,
       last_sender_username,
       last_ts_ms,
       unread_count,
       last_read_msg_id,
       last_read_at_ms
    FROM conversations
    WHERE owner_account_id = ?
    ORDER BY last_ts_ms DESC
    LIMIT ?)";
    auto result=conn->queryPrepared(sql,{ownerAccountId,limit});
    if(!result.ok()){
        return {};
    }
    if(result.rows.empty()){
        return {};
    }
    std::vector<ConversationSummary> summarys;
    for(const auto& row:result.rows){
        ConversationSummary summary;
        summary.ownerAccountId=getString(row,"owner_account_id");
        summary.type=static_cast<ConversationType>(getInt(row,"conversation_type"));
        summary.targetId=getString(row,"target_id");
        summary.lastMsgId=getUInt64(row,"last_msg_id");
        summary.lastPreview=getString(row,"last_preview");
        summary.lastSenderAccountId=getString(row,"last_sender_account_id");
        summary.lastSenderUsername=getString(row,"last_sender_username");
        summary.lastTsMs=getUInt64(row,"last_ts_ms");
        summary.lastReadMsgId=getUInt64(row,"last_read_msg_id");
        summary.lastReadAtMs=getUInt64(row,"last_read_at_ms");
        summary.unreadCount=static_cast<uint32_t>(getUInt64(row,"unread_count"));
        summarys.emplace_back(std::move(summary));
    }
    return summarys;
}
storage::RepoResult storage::SqlConversationRepo::markConversationRead(const std::string& ownerAccountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,uint64_t readAtMs){
    if(ownerAccountId.empty()||targetId.empty()||readMsgId==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }
    auto result=conn->executePrepared("UPDATE conversations SET unread_count=0,last_read_msg_id=GREATEST(last_read_msg_id,?),last_read_at_ms=? WHERE owner_account_id=? AND conversation_type=? AND target_id=?",{readMsgId,readAtMs,ownerAccountId,static_cast<uint64_t>(type),targetId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.affectedRows==0){//幂ok
        return {.status=RepoStatus::Ok};
    }
    return {.status=RepoStatus::Ok};
}