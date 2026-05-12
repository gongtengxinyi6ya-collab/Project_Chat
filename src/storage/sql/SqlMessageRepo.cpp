#include "storage/sql/SqlMessageRepo.h"

storage::SqlMessageRepo::SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::SaveMessageResult storage::SqlMessageRepo::saveGroupMessage(const std::string&groupId,const std::string&from,const std::string& content,uint64_t serverTsmS){
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
        auto result=conn->execute("INSERT INTO group_messages(group_id,`from`,content,server_ts) VALUES('"+groupId+"','"+from+"','"+content+"',"+std::to_string(serverTsmS)+")");
         if(result.ok()){
            return SaveMessageResult{.status=RepoStatus::Ok,.messageId=result.lastInsertId};
        }
        return SaveMessageResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return SaveMessageResult{.status=RepoStatus::SqlError,.message="Failed to connect to the database"};
}
