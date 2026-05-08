#pragma once
#include <unordered_set>
#include <string>
#include <mutex>
#include "UserRepo.h"
namespace storage{
class MemoryUserRepo:public storage::UserRepo{
public:
    RepoResult createUser(const std::string& username)override;//注册或首次登录时创建用户
    bool userExists(const std::string& username)override;//检查用户是否存在
private:
    std::unordered_set<std::string> users_;//用户集合
    mutable std::mutex mutex_;//保护users_的读写

};
}