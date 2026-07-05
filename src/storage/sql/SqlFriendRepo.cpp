#include "storage/sql/SqlFriendRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include "storage/sql/SqlErrorMapper.h"

storage::SqlFriendRepo::SqlFriendRepo(std::shared_ptr<SqlConnectionPool> pool):
pool_(std::move(pool)){

}
storage::RepoResult storage::SqlFriendRepo::addFriendPair(const std::string& accountId,const std::string& friendAccountId,int64_t createAtMs)
{
    //校验账号非空
    if(accountId.empty()||friendAccountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    //禁止自己加自己
    if(accountId==friendAccountId){
        return RepoResult{.status=RepoStatus::CannotAddYourself,.message="Can not add yourself"};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(!conn->connected()){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to connect to database"};
    }
   
    
    try{
         //开启事务
        SqlTransaction transaction(*conn);//开启事务
        //插入双向关系；
        auto result1=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,created_at_ms,status) VALUES(?,?,?,1) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{accountId,friendAccountId,createAtMs,createAtMs});
        auto status=mapSqlErrorToRepoStatus(result1);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="friend already exiest"};
        }
        auto result2=conn->executePrepared("INSERT INTO friend_relations(account_id,friend_account_id,created_at_ms,status) VALUES(?,?,?,1) ON DUPLICATE KEY UPDATE status=1,created_at_ms=?",{friendAccountId,accountId,createAtMs,createAtMs});
        auto status=mapSqlErrorToRepoStatus(result2);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="User already exiest"};
        }
        if(result1.ok()&&result2.ok()){
            transaction.commit();
            return RepoResult{.status=RepoStatus::Ok};
        }
        return RepoResult{.status=RepoStatus::SqlError};
    }catch(const std::exception&e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }

}
storage::RepoResult storage::SqlFriendRepo::removeFriendPair(const std::string& accountId,const std::string& friendAccountId){
    if(accountId.empty()||friendAccountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    //禁止自己加自己
    if(accountId==friendAccountId){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="Can not remove yourself"};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(!conn->connected()){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to connect to database"};
    }
    try{
        //开启事务
        SqlTransaction transaction(*conn);
        //软删除双向关系
        auto result1=conn->executePrepared("UPDATE friend_relations SET status=2 WHERE account_id=? AND friend_account_id=? AND status=1",{accountId,friendAccountId});
        if(!result1.ok()||result1.affectedRows==0){
            return RepoResult{.status=RepoStatus::NotFound,.message=result1.error};
        }
        auto result2=conn->executePrepared("UPDATE friend_relations SET status=2 WHERE account_id=? AND friend_account_id=? AND status=1",{friendAccountId,accountId});
        if(!result2.ok()||result2.affectedRows==0){
            return RepoResult{.status=RepoStatus::NotFound,.message=result2.error};
        }
        transaction.commit();
        return RepoResult{.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }
    
}

bool storage::SqlFriendRepo::areFriends(const std::string&accountId,const std::string& friendAccountId)const{
    if(accountId.empty()||friendAccountId.empty()){
        return false;
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return false;
    }
    auto result=conn->queryPrepared("SELECT id FROM friend_relations WHERE account_id=? AND friend_account_id=? AND status=1 LIMIT 1",{accountId,friendAccountId});
    if(result.ok()&&!result.rows.empty()){
        return true;
    }
    return false;
}

std::vector<std::string> storage::SqlFriendRepo::listFriendAccountIds(const std::string& accountId)const{
    if(accountId.empty()){
        return {};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    //查询指定用户好友列表
    auto result=conn->queryPrepared("SELECT friend_account_id FROM friend_relations WHERE account_id=? AND status=1 ORDER BY id ASC",{accountId});
    if(result.ok()&&!result.rows.empty()){
        std::vector<std::string> friendAccountIds;
        for(const auto& row:result.rows){
            auto it=row.find("friend_account_id");
            if(it!=row.end()){
                friendAccountIds.emplace_back(it->second);
            }
        }
        return friendAccountIds;
    }
    return {};
}