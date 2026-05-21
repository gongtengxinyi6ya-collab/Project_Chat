#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
storage::SqlOfflineMessageRepo::SqlOfflineMessageRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}

storage::RepoResult storage::SqlOfflineMessageRepo::saveOfflineMessage(const std::string& username,uint64_t msgId,const std::string& groupId){
    if(username.empty()||groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="Invalid argument"};
    }
    auto conn=pool_->acquire();//获取链接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to require a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO offline_messages(username,msg_id,group_id) VALUES(?,?,?)",{username,msgId,groupId});
        if(!result.ok()&&result.error.find("Duplicate entry")!=std::string::npos){
            //重复保存离线索引作为幂等处理
            return RepoResult{.status=RepoStatus::Ok,.message="Offline message already exists"};
        }
        if(!result.ok()){
            return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
        }
    }
    return RepoResult{.status=RepoStatus::Ok,.message="Offline message saved successfully"};
}
std::vector<storage::OfflineMessageIndex> storage::SqlOfflineMessageRepo::listOfflineMessage(const std::string& username,size_t limit){
    if(username.empty()){
        return {};
    }
    
}