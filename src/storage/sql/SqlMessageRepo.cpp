#include "storage/sql/SqlMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
storage::SqlMessageRepo::SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::SaveMessageResult storage::SqlMessageRepo::saveGroupMessage(uint64_t msgId,const std::string&groupId,const std::string&senderAccountId,const std::string&senderUsername,const std::string& content,uint64_t serverTsmS){
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
std::vector<storage::MessageRepo::MessageRecord> storage::SqlMessageRepo::listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit){
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

storage::SaveMessageResult storage::SqlMessageRepo::saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs){
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
std::vector<storage::MessageRepo::DirectMessageRecord> storage::SqlMessageRepo::listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit){
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
    std::vector<MessageRepo::DirectMessageRecord> messages;
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

std::vector<storage::MessageRepo::DirectMessageRecord> storage::SqlMessageRepo::listDirectMessagesAfter(const std::string& conversationKey,uint64_t lastMsgId,size_t limit){
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
    std::vector<MessageRepo::DirectMessageRecord> messages;
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
std::vector<storage::MessageRepo::MessageRecord> storage::SqlMessageRepo::listGroupMessagesAfter(const std::string& groupId,uint64_t lastMsgId,size_t limit){
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