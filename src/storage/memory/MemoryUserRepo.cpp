#include "storage/memory/MemoryUserRepo.h"

storage::RepoResult storage::MemoryUserRepo::createUser([[maybe_unused]]const std::string& accountId,const std::string&username,[[maybe_unused]]const std::string& password,[[maybe_unused]]const std::string& passwordSalt){
     storage::
    RepoResult result;
    if(username.empty()){
        result.status=RepoStatus::InvalidArgument;
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(userExists(username)){
            result.status=RepoStatus::AlreadyExists;
            return result;
        }
        users_.insert(username);
    }
    result.status=RepoStatus::Ok;
    return result;

}
bool storage::MemoryUserRepo::userExists(const std::string& username){
        auto it=users_.find(username);
        if(it!=users_.end()){
            return true;
        }
        return false;
}
std::optional<storage::UserAuthInfo> storage::MemoryUserRepo::findAuthInfoByAccountId(const std::string& accountId)const{
    std::lock_guard<std::mutex> lock(mutex_);
    if(users_.find(accountId)!=users_.end()){
        UserAuthInfo info;
        info.accountId=accountId;
        return info;
    }
    return std::nullopt;
}
std::optional<storage::UserAuthInfo> storage::MemoryUserRepo::findByUserId([[maybe_unused]]uint64_t userId)const{
    return std::nullopt;
}