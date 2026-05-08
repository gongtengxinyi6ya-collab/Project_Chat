#pragma once
#include <string>
#include "storage/RepoResult.h"

/*
管理用户持久化接口*/
namespace storage{
class UserRepo{
public:
    virtual ~UserRepo()=default;
    virtual storage::RepoResult createUser(const std::string& username)=0;//注册或者首次登录时创建用户
    virtual bool userExists(const std::string& username)=0;//判断用户是否存在
};
}