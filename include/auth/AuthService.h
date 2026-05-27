#pragma once
#include <memory>
#include <optional>
#include <string>
#include <cstdint>
#include "security/PasswordHasher.h"
#include "storage/RepoResult.h"
#include "storage/UserRepo.h"
#include "auth/AuthResult.h"
#include "storage/UserSessionRepo.h"
#include "security/TokenManager.h"
/*AuthService:
负责账号注册和登录*/
namespace auth{
class AuthService{
public:
    explicit AuthService(std::shared_ptr<storage::UserRepo> userRepo,security::PasswordHasher passwordHasher,security::TokenManager tokenManager,std::shared_ptr<storage::UserSessionRepo> userSessionRepo);
    AuthResult registerUser(const std::string& username,const std::string& password);//注册新用户
    AuthResult login(const std::string& username,const std::string& password);
    bool validatePasswordStrength(const std::string& password)const;//检查密码强度，长度和复杂度要求
    AuthResult loginByToken(const std::string& rawToken);//使用token恢复登录身份
    LogoutResult logout(const std::string& rawToken);//注销当前token
private:
    std::shared_ptr<storage::UserRepo> userRepo_;//读写用户账号数据
    security::PasswordHasher passwordHasher_;//生成密码hash,校验密码

    //token
    std::shared_ptr<storage::UserSessionRepo> userSessionRepo_;//保存与查询token会话
    security::TokenManager tokenManager_;//生成与哈希token
    
};

}