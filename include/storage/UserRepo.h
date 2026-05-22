#pragma once
#include <string>
#include <cstdint>
#include "storage/RepoResult.h"

/*
管理用户持久化接口*/
namespace storage{
struct UserAuthInfo
{
    uint64_t userId;//用户内部Id
    std::string username;
    std::string passwordHash;//数据库中密码hash
    std::string passwordSalt;//数据库中salt
    int status;//账号状态
};
class UserRepo{
//登录时从数据库中取出用户认证信息

public:
    virtual ~UserRepo()=default;
    virtual storage::RepoResult createUser(const std::string& username,const std::string& passwordHash,const std::string& passwordSalt)=0;//注册或者首次登录时创建用户
    virtual bool userExists(const std::string& username)=0;//判断用户是否存在
    virtual std::optional<UserAuthInfo> findAuthInfo(const std::string& username)=0;//登录时查询用户认证信息
};
}