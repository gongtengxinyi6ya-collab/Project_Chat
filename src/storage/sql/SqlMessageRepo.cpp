#include "storage/sql/SqlMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
#include "common/ConversationKey.h"
#include <stdexcept>

namespace storage{

std::vector<std::uint64_t> normalizeIds(std::vector<std::uint64_t> ids){
    std::sort(ids.begin(),ids.end());
    ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    return ids;
}

std::string encodeIdsAsJson(const std::vector<std::uint64_t>& ids){
    nlohmann::json json(ids);
    return json.dump();
}
SqlMessageRepo::SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
SaveMessageResult SqlMessageRepo::saveGroupMessage(uint64_t msgId,const std::string&groupId,const std::string&senderAccountId,const std::string&senderUsername,const std::string& content,uint64_t serverTsmS){
    if(groupId.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="groupId is empty"};
    }
    if(senderAccountId.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="senderAccountId is empty"};
    }
    if(content.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="content is empty"};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return SaveMessageResult{.status=RepoStatus::SqlError,.message="Failed to acquire a SqlConnection"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO messages(msg_id, group_id, sender_account_id, sender_username, content, server_ts_ms) VALUES(?,?,?,?,?,?)",{msgId,groupId,senderAccountId,senderUsername,content,serverTsmS});
         if(result.ok()){
            return SaveMessageResult{.status=RepoStatus::Ok,.messageId=msgId};
        }
        return SaveMessageResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return SaveMessageResult{.status=RepoStatus::SqlError,.message="Failed to connect to the database"};
}
std::vector<MessageRecord> SqlMessageRepo::listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit){
    if(groupId.empty()){
        return {};
    }
    if(limit==0){
        limit=20;
    }
    if(limit>100){
        limit=100;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return {};
    }
    if(conn->connected()){
        SqlResult result;
        if(beforeMsgId==0)
            result=conn->queryPrepared("SELECT msg_id,group_id,sender_account_id,sender_username,content,server_ts_ms FROM messages WHERE group_id=? ORDER BY msg_id DESC LIMIT ?",{groupId,limit});
        else
            result=conn->queryPrepared("SELECT msg_id,group_id,sender_account_id,sender_username,content,server_ts_ms FROM messages WHERE group_id=? AND msg_id<? ORDER BY msg_id DESC LIMIT ?",{groupId,beforeMsgId,limit});
        if(result.ok()){
                std::vector<MessageRecord> messages;
                for(auto& row:result.rows){
                    MessageRecord message;
                    message.messageId=getUInt64(row,"msg_id");//数据库结果从string转为uint64_t
                    message.groupId=getString(row,"group_id");
                    message.senderAccountId=getString(row,"sender_account_id");
                    message.senderUsername=getString(row,"sender_username");
                    message.content=getString(row,"content");
                    message.serverTsMs=getUInt64(row,"server_ts_ms");
                    messages.emplace_back(std::move(message));
                }
                return messages;
            }
            return {};
        }
    return {};
}

SaveMessageResult SqlMessageRepo::saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs){
    if(msgId==0||conversationKey.empty()||senderAccountId.empty()||receiverAccountId.empty()||content.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connected the Database"};
    }
    auto result=conn->executePrepared("INSERT INTO direct_messages (msg_id,conversation_key,sender_account_id,receiver_account_id,sender_username,content,server_ts_ms) VALUES(?,?,?,?,?,?,?)",{msgId,conversationKey,senderAccountId,receiverAccountId,senderUsername,content,serverTsMs});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    return {.status=RepoStatus::Ok,.messageId=msgId};

}
std::vector<DirectMessageRecord> SqlMessageRepo::listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit){
    if(conversationKey.empty()){
        return {};
    }
    if(limit>200){//限制limit
        limit=200;
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    SqlResult result;
    if(beforeMsgId==0){
        result=conn->queryPrepared("SELECT msg_id,conversation_key,sender_account_id,receiver_account_id,sender_username,content,server_ts_ms FROM direct_messages WHERE conversation_key=? ORDER BY msg_id DESC LIMIT ?",{conversationKey,limit});
    }
    else{
        result=conn->queryPrepared("SELECT msg_id,conversation_key,sender_account_id,receiver_account_id,sender_username,content,server_ts_ms FROM direct_messages WHERE conversation_key=? AND msg_id<? ORDER BY msg_id DESC LIMIT ?",{conversationKey,beforeMsgId,limit});
    }
    if(!result.ok()){
        return {};
    }
    if(result.rows.empty()){
        return {};
    }
    std::vector<DirectMessageRecord> messages;
    for(auto& row:result.rows){
        DirectMessageRecord message;
        message.conversationKey=getString(row,"conversation_key");
        message.messageId=getUInt64(row,"msg_id");//数据库结果从string转为uint64_t
        message.senderAccountId=getString(row,"sender_account_id");
        message.receiverAccountId=getString(row,"receiver_account_id");
        message.senderUsername=getString(row,"sender_username");
        message.content=getString(row,"content");
        message.serverTsMs=getUInt64(row,"server_ts_ms");
        messages.emplace_back(std::move(message));
    }
    return messages;
}

std::vector<DirectMessageRecord> SqlMessageRepo::listDirectMessagesAfter(const std::string& conversationKey,uint64_t lastMsgId,size_t limit){
    if(conversationKey.empty()){
        return {};
    }
    if(limit==0){
        limit=20;
    }
    if(limit>200){//限制limit
        limit=200;
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    SqlResult result;
    result=conn->queryPrepared("SELECT msg_id,conversation_key,sender_account_id,receiver_account_id,sender_username,content,server_ts_ms FROM direct_messages WHERE conversation_key=? AND msg_id>? ORDER BY msg_id ASC LIMIT ?",{conversationKey,lastMsgId,limit});
    if(!result.ok()){
        return {};
    }
    if(result.rows.empty()){
        return {};
    }
    std::vector<DirectMessageRecord> messages;
    for(auto& row:result.rows){
        DirectMessageRecord message;
        message.conversationKey=getString(row,"conversation_key");
        message.messageId=getUInt64(row,"msg_id");//数据库结果从string转为uint64_t
        message.senderAccountId=getString(row,"sender_account_id");
        message.receiverAccountId=getString(row,"receiver_account_id");
        message.senderUsername=getString(row,"sender_username");
        message.content=getString(row,"content");
        message.serverTsMs=getUInt64(row,"server_ts_ms");
        messages.emplace_back(std::move(message));
    }
    return messages;
}
std::vector<MessageRecord> SqlMessageRepo::listGroupMessagesAfter(const std::string& groupId,uint64_t lastMsgId,size_t limit){
    if(groupId.empty()){
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
    auto result=conn->queryPrepared("SELECT msg_id,group_id,sender_account_id,sender_username,content,server_ts_ms FROM messages WHERE group_id=? AND msg_id>? ORDER BY msg_id ASC LIMIT ?",{groupId,lastMsgId,limit});
    if(!result.ok()){
        return {};
    }
    std::vector<MessageRecord> messages;
    for(auto& row:result.rows){
        MessageRecord message;
        message.messageId=getUInt64(row,"msg_id");//数据库结果从string转为uint64_t
        message.groupId=getString(row,"group_id");
        message.senderAccountId=getString(row,"sender_account_id");
        message.senderUsername=getString(row,"sender_username");
        message.content=getString(row,"content");
        message.serverTsMs=getUInt64(row,"server_ts_ms");
        messages.emplace_back(std::move(message));
    }
    return messages;
}

RepoValueResult<MessageAckResult> SqlMessageRepo::markDeliveredBatch(const std::string&accountId,const std::vector<uint64_t>& msgIds,int64_t deliveredAtMs){
    if(accountId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }
    //对msgIds排序去重
    auto clearIds=normalizeIds(msgIds);
    //转换为JSON数组
    auto idsJson=encodeIdsAsJson(clearIds);
    try{
        //开启事务
        SqlTransaction transation(*conn);
        //批量查询合法消息
        std::string sql=R"(
        SELECT COUNT(DISTINCT ids.msg_id) AS eligible_count
        FROM JSON_TABLE(
            CAST(? AS JSON),
            '$[*]' COLUMNS(
                msg_id BIGINT UNSIGNED PATH '$'
            )
        ) AS ids
        JOIN direct_messages AS dm
        ON dm.msg_id = ids.msg_id
        AND dm.receiver_account_id = ?;
        )";
        auto result=conn->queryPrepared("message_ack.count_deliverable",sql,{idsJson,accountId});
        if(!result.ok()){
            return {.status=RepoStatus::SqlError,.message=result.error};
        }
        if(result.rows.empty()){
            return {.status=RepoStatus::NotFound,.message=result.error};
        }
        auto row=result.rows.front();
        auto count=getUInt64(row,"eligible_count");
        //批量写入回执
        auto upsertResult=conn->executePrepared("message_ack.upsert_delivered",
        R"(
        INSERT INTO message_receipts (
            msg_id,
            account_id,
            delivered_at_ms,
            read_at_ms
        )
        SELECT
            ids.msg_id,
            ?,
            ?,
            0
        FROM JSON_TABLE(
            CAST(? AS JSON),
            '$[*]' COLUMNS(
                msg_id BIGINT UNSIGNED PATH '$'
            )
        ) AS ids
        JOIN direct_messages AS dm
        ON dm.msg_id = ids.msg_id
        AND dm.receiver_account_id = ?
        ON DUPLICATE KEY UPDATE
            delivered_at_ms = GREATEST(
                message_receipts.delivered_at_ms,
                VALUES(delivered_at_ms)
            );
        )",{accountId,deliveredAtMs,idsJson,accountId});
        if(!upsertResult.ok()){
            return {.status=RepoStatus::SqlError,.message=upsertResult.error};
        }
        if(result.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=upsertResult.error};
        }
        //提交事务
        transation.commit();
        return {.status=RepoStatus::Ok,.value=MessageAckResult{.requestedCount=count,.ackedCount=upsertResult.affectedRows,.ignoredCount=count-upsertResult.affectedRows}};
    }catch(const std::exception&e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }

}
RepoValueResult<size_t> SqlMessageRepo::markReadBefore(const std::string&accountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,int64_t readAtMs){
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }
    std::string sql;
    SqlResult result;
    if(type==ConversationType::Direct){
        auto conversationKey=common::buildDirectConversationKey(accountId,targetId);
        sql=R"(
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
        WHERE conversation_key=?
        AND msg_id <= ?
        AND receiver_account_id=?
        ON DUPLICATE KEY UPDATE
            delivered_at_ms = GREATEST(delivered_at_ms, VALUES(delivered_at_ms)),
            read_at_ms = GREATEST(read_at_ms, VALUES(read_at_ms))
        )";
        result=conn->executePrepared(sql,{accountId,readAtMs,readAtMs,conversationKey,readMsgId,accountId});
        
    }
    else if(type==ConversationType::Group){
        sql=R"(
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
        FROM messages
        WHERE  group_id = ?
        AND msg_id <= ?
        AND sender_account_id <> ?
        ON DUPLICATE KEY UPDATE
            delivered_at_ms = GREATEST(delivered_at_ms, VALUES(delivered_at_ms)),
            read_at_ms = GREATEST(read_at_ms, VALUES(read_at_ms))
        )";
        result=conn->executePrepared(sql,{accountId,readAtMs,readAtMs,targetId,readMsgId,accountId});
    }
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    return {.status=RepoStatus::Ok,.value=static_cast<size_t>(result.affectedRows)};
}

}