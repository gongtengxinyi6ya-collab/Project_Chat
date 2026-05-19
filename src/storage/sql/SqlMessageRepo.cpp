#include "storage/sql/SqlMessageRepo.h"

storage::SqlMessageRepo::SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::SaveMessageResult storage::SqlMessageRepo::saveGroupMessage(uint64_t msgId,const std::string&groupId,const std::string&from,const std::string& content,uint64_t serverTsmS){
    if(groupId.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="groupId is empty"};
    }
    if(from.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="from is empty"};
    }
    if(content.empty()){
        return SaveMessageResult{.status=RepoStatus::InvalidArgument,.message="content is empty"};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return SaveMessageResult{.status=RepoStatus::SqlError,.message="Failed to acquire a SqlConnection"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO messages(msg_id, group_id, sender, content, server_ts_ms) VALUES(?,?,?,?,?)",{msgId,groupId,from,content,serverTsmS});
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
    auto conn=pool_->acquire();
    if(!conn){
        return {};
    }
    if(conn->connected()){
        SqlResult result;
        if(beforeMsgId==0)
            result=conn->queryPrepared("SELTCT msg_id,group_id,sender,content,server_ts_ms FROM messages WHERE group_id=? ORDER BY msg_id DESC LIMIT ?",{groupId,limit});
        else
            result=conn->queryPrepared("SELECT msg_id,groupId,sender,content,server_ts_ms FROM messages WHERE group_id=? AND msg_id<? ORDER BY msg_id DESC LIMIT >",{groupId,beforeMsgId,limit});
        if(result.ok()){
                std::vector<MessageRecord> messages;
                for(auto& row:result.rows){
                    MessageRecord message;
                    auto groupIdPair=row.find("group_id");
                    message.groupId=groupIdPair!=row.end()?groupIdPair->second:"";
                    auto msgIdPair=row.find("msg_id");
                    message.messageId=msgIdPair!=row.end()?std::stoull(msgIdPair->second):0;//数据库结果从string转为uint64_t
                    auto senderPair=row.find("sender");
                    message.from=senderPair!=row.end()?senderPair->second:"";
                    auto contentPair=row.find("content");
                    message.content=contentPair!=row.end()?contentPair->second:"";
                    auto serverTsMsPair=row.find("server_ts_ms");
                    message.serverTsMs=serverTsMsPair!=row.end()?std::stoull(serverTsMsPair->second):0;
                    messages.emplace_back(std::move(message));
                }
                return messages;
            }
            return {};
        }
    return {};
}