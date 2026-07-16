#include "storage/sql/SqlGroupMessageWriteStore.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include <exception>
namespace storage{
SqlGroupMessageWriteStore::SqlGroupMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){
    if(!pool_){
        throw std::invalid_argument("invalid pool");
    }
}

RepoValueResult<std::uint64_t> SqlGroupMessageWriteStore::commit(const im::GroupMessageWriteCommand& command){
    if(command.msgId==0||command.groupId.empty()||command.senderAccountId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="failed to connect the pool"};
    }
    try{
        //开启事务处理
        SqlTransaction transation(*conn);
        //锁定对应群的会话头
        auto headResult=conn->queryPrepared("group_message.select_head_for_update",R"(
            SELECT last_seq
            FROM group_conversation_heads
            WHERE group_id = ?
            FOR UPDATE;
            )",
        {command.groupId});
        if(!headResult.ok()){
            return {.status=RepoStatus::SqlError,.message=headResult.error};
        }
        if(headResult.rows.empty()){
            return {.status=RepoStatus::NotFound,.message=headResult.error};
        }
        //读取群消息序号
        auto row=headResult.rows.front();
        auto groupSeq=getUInt64(row,"last_seq")+1;
        //插入消息
        auto insertResult=conn->executePrepared("group_message.insert_message",R"(
            INSERT INTO messages (
                msg_id,
                group_id,
                group_seq,
                sender_account_id,
                sender_username,
                content,
                server_ts_ms
            )
            VALUES (?, ?, ?, ?, ?, ?, ?);
            )",
        {command.msgId,command.groupId,groupSeq,command.senderAccountId,command.senderUsername,command.content,command.serverTsMs});
        if(!insertResult.ok()){
            return {.status=RepoStatus::SqlError,.message=insertResult.error};
        }
        if(insertResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=insertResult.error};
        }
        //更新群会话头
        std::string finalPreview=command.content;
        if(finalPreview.size()>200){
            finalPreview=finalPreview.substr(0,200);
        }
        auto updateResult=conn->executePrepared("group_message.update_head",R"(
            UPDATE group_conversation_heads
                SET
                    last_seq = ?,
                    last_msg_id = ?,
                    last_preview = ?,
                    last_sender_account_id = ?,
                    last_sender_username = ?,
                    last_ts_ms = ?
                WHERE group_id = ?;
            )",
        {groupSeq,command.msgId,finalPreview,command.senderAccountId,command.senderUsername,command.serverTsMs});
        if(!updateResult.ok()){
            return {.status=RepoStatus::SqlError,.message=updateResult.error};
        }
        if(updateResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=updateResult.error};
        }
        //更新发送者游标
        auto cursorResult=conn->executePrepared("group_message.update_sender_cursor",R"(
            UPDATE user_group_cursors
            SET
                last_read_seq = GREATEST(last_read_seq, ?),
                last_read_msg_id = ?,
                last_read_at_ms = GREATEST(last_read_at_ms, ?)
            WHERE account_id = ?
            AND group_id = ?;
            )",
        {groupSeq,command.msgId,command.serverTsMs,command.senderAccountId,command.groupId});
        if(!cursorResult.ok()){
            return {.status=RepoStatus::SqlError,.message=cursorResult.error};
        }
        if(cursorResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=updateResult.error};
        }
        
        //提交事务
        transation.commit();
        return {.status=RepoStatus::Ok,.value=groupSeq};
    }catch(const std::exception& e){
        return {.status=RepoStatus::SqlError,.message=e.what()};
    }
}   
}