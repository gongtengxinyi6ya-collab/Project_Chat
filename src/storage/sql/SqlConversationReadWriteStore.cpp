#include "storage/sql/SqlConversationReadWriteStore.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"

#include "common/ConversationKey.h"
#include <stdexcept>
#include <algorithm>
#include <utility>

namespace storage{

SqlConversationReadWriteStore::SqlConversationReadWriteStore(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){
    if(!pool_){
        throw std::invalid_argument("conversation read SQL pool is null");
    }
}
RepoValueResult<ConversationReadResult> SqlConversationReadWriteStore::commit(const ConversationReadCommand& command) {
    if (command.accountId.empty() ||command.targetId.empty() ||command.readMsgId == 0 ||command.type == ConversationType::Unknown) {
        return {.status = RepoStatus::InvalidArgument,.message = "invalid conversation read command"};
    }
    //获取连接
    auto connection = pool_->acquire();
    if (!connection || !connection->connected()) {
        return {.status = RepoStatus::SqlError,.message = "failed to acquire message SQL connection"};
    }
    //事务处理
    try {
        SqlTransaction transaction(*connection);
        RepoValueResult<ConversationReadResult> result;
        if(command.type == ConversationType::Direct){
            result = commitDirect(*connection, command);
        } 
        else if(command.type == ConversationType::Group){
            result = commitGroup(*connection, command);
        } 
        else{
            return{.status = RepoStatus::InvalidArgument,.message = "unknown conversation type"};
        }
        if (!result.ok() || !result.value) {
            return result;
        }
        transaction.commit();
        return result;
    } catch (const std::exception& exception) {
        return {.status = RepoStatus::SqlError,.message = exception.what()};
    }
}

RepoValueResult<ConversationReadResult> SqlConversationReadWriteStore::commitDirect(SqlConnection& connection,const ConversationReadCommand& command){
    //锁定会话摘要
    auto cvsResult=connection.queryPrepared("direct_conversation.select_for_update",
    R"(
        SELECT
            last_read_msg_id,
            last_read_at_ms,
            unread_count
        FROM conversations
        WHERE owner_account_id = ?
        AND conversation_type = 1
        AND target_id = ?
        FOR UPDATE;
    )",
    {command.accountId,command.targetId});
    if(!cvsResult.ok()){
        return {.status=RepoStatus::SqlError,.message=cvsResult.error};
    }
    if(cvsResult.rows.empty()){
        return {.status=RepoStatus::NotFound,.message=cvsResult.error};
    }

    //校验readMsgIds属于当前会话
    auto conversationKey=common::buildDirectConversationKey(command.accountId,command.targetId);
    auto checkResult=connection.queryPrepared("direct_messages.select_check_for_update",
    R"(
        SELECT msg_id
        FROM direct_messages
        WHERE conversation_key = ?
        AND msg_id = ?
        FOR UPDATE;
    )",{conversationKey,command.readMsgId});
    if(!checkResult.ok()){
        return {.status=RepoStatus::SqlError,.message=checkResult.error};
    }
    if(checkResult.rows.empty()){
        return {.status=RepoStatus::MessageNotFound,.message=checkResult.error};
    }
    //计算真正需要推进的游标
    auto row=cvsResult.rows.front();
    const auto oldReadMsgId=getUInt64(row,"last_read_msg_id");
    const auto oldReadAtMs=getInt64(row,"last_read_at_ms");
    const auto newReadMsgId=std::max(oldReadMsgId,command.readMsgId);
    if(newReadMsgId==oldReadMsgId){
        //重复请求或旧请求，幂等返回
        return {.status=RepoStatus::Ok,
            .value=ConversationReadResult{
            .type = ConversationType::Direct,
            .targetId = command.targetId,
            .readMsgId = oldReadMsgId,
            .readAtMs = oldReadAtMs,
            .receiptUpdated = 0
        }};
    }

    //计算本次新增已读消息数量
    auto readResult=connection.queryPrepared("direct_messages.select_count",
    R"(
        SELECT COUNT(*) AS newly_read
        FROM direct_messages
        WHERE conversation_key = ?
        AND receiver_account_id = ?
        AND msg_id > ?
        AND msg_id <= ?;
    )",
    {conversationKey,command.accountId,oldReadMsgId,newReadMsgId});
    if(!readResult.ok()){
        return {.status=RepoStatus::SqlError,.message=readResult.error};
    }
    if(readResult.rows.empty()){
        return {.status=RepoStatus::MessageNotFound,.message=readResult.error};
    }
    auto readRow=readResult.rows.front();
    auto newLyRead=getUInt64(readRow,"newly_read");
    //批量写入已读回执
    auto writeResult=connection.executePrepared("message_receipts.insert_read",
    R"(
    INSERT INTO message_receipts (
        msg_id,
        account_id,
        delivered_at_ms,
        read_at_ms
    )
    SELECT
        msg_id,
        ?,
        ?,
        ?
    FROM direct_messages
    WHERE conversation_key = ?
    AND receiver_account_id = ?
    AND msg_id > ?
    AND msg_id <= ?
    ON DUPLICATE KEY UPDATE
        delivered_at_ms = GREATEST(
            message_receipts.delivered_at_ms,
            VALUES(delivered_at_ms)
        ),
        read_at_ms = GREATEST(
            message_receipts.read_at_ms,
            VALUES(read_at_ms)
        );
    )",
    {command.accountId,command.readAtMs,command.readAtMs,conversationKey,command.accountId,oldReadMsgId,newReadMsgId});
    if(!writeResult.ok()){
        return {.status=RepoStatus::SqlError,.message=writeResult.error};
    }
    
    //更新会话游标和未读数
    auto updateResult=connection.executePrepared("conversations_update_unread",
    R"(
    UPDATE conversations
    SET
        last_read_msg_id = ?,
        last_read_at_ms = GREATEST(last_read_at_ms, ?),
        unread_count =
            CASE
                WHEN unread_count > ?
                THEN unread_count - ?
                ELSE 0
            END
    WHERE owner_account_id = ?
    AND conversation_type = 1
    AND target_id = ?;
    )",{newReadMsgId,command.readAtMs,newLyRead,newLyRead,command.accountId,command.targetId});
    if(!updateResult.ok()){
        return {.status=RepoStatus::SqlError,.message=updateResult.error};
    }
    if (updateResult.affectedRows == 0) {
        return {.status = RepoStatus::Conflict,.message ="direct conversation cursor was not updated"};
    }
    return {.status=RepoStatus::Ok,
        .value=ConversationReadResult{.type=ConversationType::Direct,
        .targetId=command.targetId,
        .readMsgId=newReadMsgId,
        .readAtMs=command.readAtMs,
        .receiptUpdated=newLyRead}};
}

RepoValueResult<ConversationReadResult> SqlConversationReadWriteStore::commitGroup(SqlConnection& connection,const ConversationReadCommand& command){
    //锁定群成员游标
    auto cursorResult=connection.queryPrepared("conversation_read.group_cursor_for_update",
        R"(
        SELECT
            last_read_seq,
            last_read_msg_id,
            last_read_at_ms
        FROM user_group_cursors
        WHERE account_id = ?
        AND group_id = ?
        FOR UPDATE;
        )",{command.accountId,command.targetId});
    if(!cursorResult.ok()){
        return {.status=RepoStatus::SqlError,.message=cursorResult.error};
    }
    if(cursorResult.rows.empty()){
        //查不到：用户退群、被题、数据不一致
        return {.status=RepoStatus::TargetNotInGroup,.message="user not in group"};
    }
    auto row=cursorResult.rows.front();
    auto oldReadSeq=getUInt64(row,"last_read_seq");
    auto oldReadMsgId=getUInt64(row,"last_read_msg_id");
    auto oldReadAtMs=getInt64(row,"last_read_at_ms");
    //获取目标消息的群内序号
    auto seqResult=connection.queryPrepared("conversation_read.group_message_for_update",
        R"(
        SELECT group_seq
        FROM messages
        WHERE group_id = ?
        AND msg_id = ?
        FOR UPDATE;
        )",
    {command.targetId,command.readMsgId});
    if(!seqResult.ok()){
        return {.status=RepoStatus::SqlError,.message=seqResult.error};
    }
    if(seqResult.rows.empty()){
        return {.status=RepoStatus::MessageNotFound,.message=seqResult.error};
    }
    auto seqRow=seqResult.rows.front();
    auto targetGroupSeq=getUInt64(seqRow,"group_seq");

    //幂等推进游标
    if(targetGroupSeq<=oldReadSeq){
        return {
        .status = RepoStatus::Ok,
        .value = ConversationReadResult{
            .type = ConversationType::Group,
            .targetId = command.targetId,
            .readMsgId = oldReadMsgId,
            .readAtMs = oldReadAtMs,
            .receiptUpdated = 0
        }
    };
    }
    //真正推进
    auto updateResult=connection.executePrepared("conversation_read.update_group_cursor",
        R"(
        UPDATE user_group_cursors
        SET
            last_read_seq = ?,
            last_read_msg_id = ?,
            last_read_at_ms = GREATEST(last_read_at_ms, ?)
        WHERE account_id = ?
        AND group_id = ?;
        )",
    {targetGroupSeq,command.readMsgId,command.readAtMs,command.accountId,command.targetId});
    if(!updateResult.ok()){
        return {.status=RepoStatus::SqlError,.message=updateResult.error};
    }
    if (updateResult.affectedRows == 0) {
        return {.status = RepoStatus::Conflict,.message ="group read cursor was not updated"};
    }

    return {.status = RepoStatus::Ok,
        .value = ConversationReadResult{
            .type = ConversationType::Group,
            .targetId = command.targetId,
            .readMsgId = command.readMsgId,
            .readAtMs = command.readAtMs,
            .receiptUpdated = 0
        }
    };
}
}