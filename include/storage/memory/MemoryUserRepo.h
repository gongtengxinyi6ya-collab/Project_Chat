#pragma once
#include <unordered_set>
#include <string>
#include <mutex>
#include "storage/UserRepo.h"
namespace storage{
class MemoryUserRepo:public storage::UserRepo{
public:
    RepoResult createUser(const std::string& accountId,const std::string& username,const std::string& password,const std::string& passwordSalt)override;//注册或首次登录时创建用户
    bool userExists(const std::string& accountId)override;//检查用户是否存在
    std::optional<UserAuthInfo> findAuthInfoByAccountId(const std::string& accountId)const override;//登录时查询用户认证信息
    std::optional<UserAuthInfo> findByUserId(uint64_t userid)const;
private:
    std::unordered_set<std::string> users_;//用户集合
    mutable std::mutex mutex_;//保护users_的读写

};
}