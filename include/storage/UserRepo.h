#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include "storage/RepoResult.h"
#include "storage/types/UserTypes.h"
/*
管理用户持久化接口*/
namespace storage{

class UserRepo{
//登录时从数据库中取出用户认证信息

public:
    virtual ~UserRepo()=default;
    virtual storage::RepoResult createUser(const std::string& accountId,const std::string& username,const std::string& passwordHash,const std::string& passwordSalt)=0;//注册或者首次登录时创建用户
    virtual bool userExists(const std::string& accountId)=0;//判断用户是否存在
    virtual std::optional<UserAuthInfo> findAuthInfoByAccountId(const std::string& accountId)const=0;//登录时查询用户认证信息
    virtual std::optional<UserAuthInfo> findByUserId(uint64_t userId) const = 0;
};
}