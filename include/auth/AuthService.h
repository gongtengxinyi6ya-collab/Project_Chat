#pragma once
#include <memory>
#include <optional>
#include <string>
#include "security/PasswordHasher.h"
#include "storage/RepoResult.h"
#include "storage/UserRepo.h"
#include "auth/AuthResult.h"
/*AuthService:
负责账号注册和登录*/
namespace auth{
class AuthService{
public:
    explicit AuthService(std::shared_ptr<storage::UserRepo> userRepo,security::PasswordHasher passwordHasher);
    AuthResult registerUser(const std::string& username,const std::string& password);//注册新用户
    AuthResult login(const std::string& username,const std::string& password);
    bool validatePasswordStrength(const std::string& password)const;//检查密码强度，长度和复杂度要求
private:
    std::shared_ptr<storage::UserRepo> userRepo_;//读写用户账号数据
    security::PasswordHasher passwordHasher_;//生成密码hash,校验密码
};

}