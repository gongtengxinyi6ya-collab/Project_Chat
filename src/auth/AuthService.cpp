#include "auth/AuthService.h"
#include <stdexcept>
#include <cctype>
auth::AuthService::AuthService(std::shared_ptr<storage::UserRepo> userRepo,security::PasswordHasher passwordHasher)
:userRepo_(std::move(userRepo)),passwordHasher_(passwordHasher){
    if(userRepo_==nullptr){
        throw std::invalid_argument("userRepo is null");
    }
}
auth::AuthResult auth::AuthService::registerUser(const std::string& username,const std::string& password){
    //校验username格式
    if(username.empty()){
        return AuthResult{.status=AuthStatus::InvalidArgument,.message="username is empty"};
    }

    //校验password格式
    if(password.empty()){
        return AuthResult{.status=AuthStatus::InvalidArgument,.message="password is empty"};
    }

    if(!validatePasswordStrength(password)){
        return AuthResult{.status=AuthStatus::WeakPassword,.message="password is too weak"};
    }
    try{
        auto hashResult=passwordHasher_.hashPassword(password);
        auto result=userRepo_->createUser(username,hashResult.hash,hashResult.salt);
        if(result.ok()){
            return AuthResult{.ok=true,.status=AuthStatus::Ok};
        }
        if(result.status==storage::RepoStatus::AlreadyExists){
            return AuthResult{.status=AuthStatus::AlreadyExist,.message="User already exists"};
        }
        return AuthResult{.status=AuthStatus::Internal,.message=result.message};
    }catch(const std::exception& e){
        return AuthResult{.status=AuthStatus::Internal,.message=e.what()};
    }

}
auth::AuthResult auth::AuthService::login(const std::string& username,const std::string& password){
    if(username.empty()||password.empty()){
        return AuthResult{.status=AuthStatus::InvalidArgument,.message="username or password is empty"};
    }
    //查询用户信息
    std::optional<storage::UserAuthInfo> result=userRepo_->findAuthInfo(username);
    if(!result){
        return AuthResult{.status=AuthStatus::UserNotFound};
    }
    if(result->status!=0){
        return AuthResult{.status=AuthStatus::UserDisabled};
    }
    //校验登录密码
    if(!passwordHasher_.verifyPassword(password,result.value().passwordHash,result.value().passwordSalt)){
        return AuthResult{.status=AuthStatus::BadPassword,.message="password is wrong"};
    }
    return AuthResult{.ok=true,.user=result.value()};
}
bool auth::AuthService::validatePasswordStrength(const std::string& password)const{
    if(password.size()<8||password.size()>32){
        return false;
    }
    bool hasUpper=false;
    bool hasLower=false;
    bool hasDigit=false;
    for(char c:password){
        if(std::isupper(static_cast<unsigned char>(c))){
            hasUpper=true;
        }
        else if(std::islower(static_cast<unsigned char>(c))){
            hasLower=true;
        }
        else if(std::isdigit(static_cast<unsigned char>(c))){
            hasDigit=true;
        }
        if(hasUpper&&hasLower&&hasDigit){
            return true;
        }
    }
    return false;
}