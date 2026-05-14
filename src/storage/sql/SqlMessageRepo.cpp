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
