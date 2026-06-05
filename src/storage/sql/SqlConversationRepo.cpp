#include "storage/sql/SqlConversationRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include <stdexcept>
storage::SqlConversationRepo::SqlConversationRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::RepoResult storage::SqlConversationRepo::upserDirectOnMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs){
    if(senderAccountId.empty()||receiverAccountId.empty()||msgId==0||serverTsMs==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    if(preview.size()>200){
        preview.substr(0,200);
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the dataBase"};
    }
    try{
        //开启事务
        SqlTransaction transation(*conn);
        const std::string sql1=R"(INSERT INTO conversations 
        (owner_account_id,conversation_type,target_id,last_msg_id,last_preview,last_sender_account_id,last_sender_username,last_ts_ms,unread_count,last_read_msg_id,last_read_at_ms)
        VALUES (?, 1, ?, ?, ?, ?, ?, ?, 0, ?, ?)
        ON DUPLICATE KEY UPDATE
            last_msg_id = VALUES(last_msg_id),
            last_preview = VALUES(last_preview),
            last_sender_account_id = VALUES(last_sender_account_id),
            last_sender_username = VALUES(last_sender_username),
            last_ts_ms = VALUES(last_ts_ms),
            unread_count = 0,
            last_read_msg_id = VALUES(last_read_msg_id),
            last_read_at_ms = VALUES(last_read_at_ms)";
        auto result1=conn->executePrepared(sql1,{senderAccountId,receiverAccountId,msgId,preview,senderAccountId,senderUsername,serverTsMs,msgId,serverTsMs});
        if(!result1.ok()){
            return {.status=RepoStatus::SqlError,.message=result1.error};
        }
        const std::string sql2=R"(
        INSERT INTO conversations (
            owner_account_id,
            conversation_type,
            target_id,
            last_msg_id,
            ast_preview,
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
            unread_count = unread_count + 1;
        )";
        auto result2=conn->executePrepared(sql2,{receiverAccountId,senderAccountId,msgId,preview,senderAccountId,senderUsername,serverTsMs});
        if(!result2.ok()){
            return {.status=RepoStatus::SqlError,.message=result2.error};
        }
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception&e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
}
