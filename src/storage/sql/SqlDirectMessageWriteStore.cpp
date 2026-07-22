#include "storage/sql/SqlDirectMessageWriteStore.h"
#include <stdexcept>
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
namespace storage{

SqlDirectMessageWriteStore::SqlDirectMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){
    if(!pool_){
        std::invalid_argument("sql pool for direct mesage write store invalid");
    }
}

RepoResult SqlDirectMessageWriteStore::commit(const im::DirectMessageWriteCommand& command){
    if(command.msgId==0||command.senderAccountId.empty()||command.receiverAccountId.empty()||command.conversationKey.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="failed to connect the pool"};
    }
    try{
        SqlTransaction transation;

        //检查接收账号
        auto checkReciveResult=conn->queryPrepared("direct_message.check_receiver",
        R"(
        SELECT 1
        FROM user_profiles
        WHERE account_id = ?
        LIMIT 1;
        )",{command.receiverAccountId}
        );
        if(!checkReciveResult.ok()){
            return {.status=RepoStatus::SqlError,.message=checkReciveResult.error};
        }
        if(checkReciveResult.rows.empty()){
            return {.status=RepoStatus::NotFound,.message=checkReciveResult.error};
        }

        //检查好友关系
        auto checkRelationResult=conn->queryPrepared("direct_message.check_friend",
        R"(
        SELECT 1
        FROM friend_relations
        WHERE account_id = ?
        AND friend_account_id = ?
        AND status = 1
        LIMIT 1;
        )",
        {command.senderAccountId,command.receiverAccountId});
        if(checkRelationResult.ok()){
            return {.status=RepoStatus::SqlError,.message=checkRelationResult.error};
        }
        if(checkRelationResult.rows.empty()){
            return {.status=RepoStatus::NotFriends,.message=checkRelationResult.error};
        }

        //保存私聊消息
        auto saveResult=conn->executePrepared("direct_message.insert_message",
        R"(
        INSERT INTO direct_messages (
            msg_id,
            conversation_key,
            sender_account_id,
            receiver_account_id,
            sender_username,
            content,
            server_ts_ms
        )
        VALUES (?, ?, ?, ?, ?, ?, ?);
        )",
        {command.msgId,command.conversationKey,command.senderAccountId,
        command.receiverAccountId,command.senderUsername,
        command.content,command.serverTsMs});
        if(!saveResult.ok()){
            return {.status=RepoStatus::SqlError,.message=saveResult.error};
        }
        if(saveResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=saveResult.error};
        }
        //更新发送方会话
        std::string finalPreview=command.content;
        if(finalPreview.size()>200){
            finalPreview=finalPreview.substr(0,200);
        }
        auto upsertSenderResult=conn->executePrepared("direct_message.upsert_sender_conversation",
        R"(
        INSERT INTO conversations (
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
        )
        VALUES (?, 1, ?, ?, ?, ?, ?, ?, 0, ?, ?)
        ON DUPLICATE KEY UPDATE
            last_msg_id = VALUES(last_msg_id),
            last_preview = VALUES(last_preview),
            last_sender_account_id = VALUES(last_sender_account_id),
            last_sender_username = VALUES(last_sender_username),
            last_ts_ms = VALUES(last_ts_ms),
            unread_count = 0,
            last_read_msg_id = VALUES(last_read_msg_id),
            last_read_at_ms = VALUES(last_read_at_ms);
        )",
        {command.senderAccountId,command.conversationKey,
        command.msgId,finalPreview,command.senderAccountId,
        command.senderUsername,command.serverTsMs,
        command.msgId,command.serverTsMs});
        if(!upsertSenderResult.ok()){
            return {.status=RepoStatus::SqlError,.message=upsertSenderResult.error};
        }
        if(upsertSenderResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=upsertSenderResult.error};
        }

        //更新接收方会话
        std::string lastPreview=command.content;
        if(lastPreview.size()>200){
            lastPreview=lastPreview.substr(0,200);
        }
        auto upsertReceiverResult=conn->executePrepared("direct_message.upsert_receiver_conversation",
        R"(
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
            unread_count = unread_count + 1;
        )",
        {command.receiverAccountId,command.conversationKey,
        command.msgId,finalPreview,command.senderAccountId,
        command.senderUsername,command.serverTsMs,
        });
        if(!upsertReceiverResult.ok()){
            return {.status=RepoStatus::SqlError,.message=upsertReceiverResult.error};
        }
        if(upsertReceiverResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=upsertReceiverResult.error};
        }

        //创建待投递索引
        auto offlineResult=conn->executePrepared("direct_message.insert_pending_delivery",
        R"(
        INSERT INTO offline_messages (
            account_id,
            msg_type,
            msg_id,
            peer_account_id,
            group_id
        )
        VALUES (?, 2, ?, ?, NULL)
        ON DUPLICATE KEY UPDATE
            peer_account_id = VALUES(peer_account_id);
        )",
        {command.receiverAccountId,command.msgId,command.conversationKey
        });
        if(!offlineResult.ok()){
            return {.status=RepoStatus::SqlError,.message=offlineResult.error};
        }
        if(upsertReceiverResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=upsertReceiverResult.error};
        }
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return {.status=RepoStatus::SqlError,.message=e.what()};
    }
}
}