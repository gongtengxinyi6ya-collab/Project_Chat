#include "storage/sql/SqlUserRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionGuard.h"
storage::SqlUserRepo::SqlUserRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::RepoResult storage::SqlUserRepo::createUser(const std::string& accountId,const std::string& username,const std::string&passwordHash,const std::string& passwordSalt){
    if(username.empty()||passwordHash.empty()||passwordSalt.empty()||accountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="argument invalid"};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){//获取连接失败
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a SqlConnection"};
    }
    auto result=conn->executePrepared("INSERT INTO users(account_id,username,password_hash,password_salt) VALUES(?,?,?,?)",{accountId,username,passwordHash,passwordSalt});

    if(result.ok()){
        return RepoResult{.status=RepoStatus::Ok};
    }
    if(result.error.find("Duplicate entry")!=std::string::npos){//唯一键冲突
        return RepoResult{.status=RepoStatus::AlreadyExists,.message="Account already exists"};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
}
bool storage::SqlUserRepo::userExists(const std::string& accountId){
    if(accountId.empty()){
        return false;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return false;
    }
    auto result=conn->queryPrepared("SELECT id FROM users WHERE account_id=? LIMIT 1",{accountId});
    if(!result.ok()||result.rows.empty()){
        return false;
    }
    return true;
}
std::optional<storage::UserAuthInfo> storage::SqlUserRepo::findAuthInfoByAccountId(const std::string& accountId)const{
    if(accountId.empty()){
        return std::nullopt;
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return std::nullopt;
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT id,account_id,username,password_hash,password_salt,status FROM users WHERE account_id=? LIMIT 1",{accountId});
        if(!result.ok()||result.rows.empty()){
            return std::nullopt;
        }
        UserAuthInfo info;
        const auto& row=result.rows.front();
        auto idPair=row.find("id");
        info.userId=idPair!=row.end()?std::stoull(idPair->second):0;
        auto usernamePair=row.find("username");
        info.username=usernamePair!=row.end()?usernamePair->second:"";
        auto accountPair=row.find("account_id");
        info.accountId=accountPair!=row.end()?accountPair->second:"";
        auto hashPair=row.find("password_hash");
        info.passwordHash=hashPair!=row.end()?hashPair->second:"";
        auto saltPair=row.find("password_salt");
        info.passwordSalt=saltPair!=row.end()?saltPair->second:"";
        auto statusPair = row.find("status");
        info.status = statusPair != row.end() ? std::stoi(statusPair->second) : 0;
        return info;
    }
    return std::nullopt;
}