#include "storage/sql/SqlUserRepo.h"

storage::SqlUserRepo::SqlUserRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::RepoResult storage::SqlUserRepo::createUser(const std::string& username){
    if(username.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="username invalid"};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){//获取连接失败
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a SqlConnection"};
    }
    auto result=conn->execute("INSERT INTO users(username) VALUES(username)");
    if(result.ok()){
        return RepoResult{.status=RepoStatus::Ok};
    }
    if(result.error.find("Duplicate entry")!=std::string::npos){//唯一键冲突
        return RepoResult{.status=RepoStatus::AlreadyExists,.message="User already exists"};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
}
bool storage::SqlUserRepo::userExists(const std::string& username){
    if(username.empty()){
        return false;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return false;
    }
    auto result=conn->query("SELECT id FROM users WHERE username='"+username+"'");
    if(!result.rows.empty()){
        return false;
    }
    return true;
}