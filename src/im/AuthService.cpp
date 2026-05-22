#include "im/AuthService.h"

storage::RepoResult im::AuthService::registerUser(const std::string& username,const std::string& password){
    //校验username格式
    if(username.empty()){
        return storage::RepoResult{.status=storage::RepoStatus::InvalidArgument,.message="username is empty"};
    }

    //校验password格式
    if(password.empty()){
        return storage::RepoResult{.status=storage::RepoStatus::InvalidArgument,.message="password is empty"};
    }

    if(!validatePasswordStrength(password)){
        return storage::RepoResult{.status=storage::RepoStatus::InvalidArgument,.message="password is too weak"};
    }
    auto hashResult=passwordHasher_.hashPassword(password);
    if(hashResult.hash.empty()||hashResult.salt.empty()){
        return storage::RepoResult{.status=storage::RepoStatus::InvalidArgument,.message="Failed to hash password"};
    }
    return userRepo_->createUser(username,hashResult.hash,hashResult.salt);
}