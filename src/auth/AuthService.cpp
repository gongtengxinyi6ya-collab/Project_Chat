#include "auth/AuthService.h"
#include "auth/AccountIdGenerator.h"
#include <stdexcept>
#include <chrono>
#include <cctype>
auth::AuthService::AuthService(std::shared_ptr<storage::UserRepo> userRepo,security::PasswordHasher passwordHasher,security::TokenManager tokenManager,std::shared_ptr<storage::UserSessionRepo> userSessionRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo)
:userRepo_(std::move(userRepo)),passwordHasher_(passwordHasher),userSessionRepo_(std::move(userSessionRepo)),tokenManager_(tokenManager),userProfileRepo_(std::move(userProfileRepo)){
    if(userRepo_==nullptr||userSessionRepo_==nullptr||userProfileRepo_==nullptr){
        throw std::invalid_argument("One or more repositories are null");
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
        for(int i=0;i<5;i++){
            auto accountId=AccountIdGenerator::generateAccountId(12);
            auto result=userRepo_->createUser(accountId,username,hashResult.hash,hashResult.salt);
            if(result.ok()){
                auto userInfo=userRepo_->findAuthInfoByAccountId(accountId);
            if(!userInfo){
                return AuthResult{.status=AuthStatus::UserNotFound};
            }
            auto createProfile=userProfileRepo_->createDefaultProfile(userInfo.value().userId,userInfo.value().accountId,username);
            if(!createProfile.ok()){
                return AuthResult{.status=AuthStatus::Internal,.message="Failed to create userProfile"};
                
            }
            return AuthResult{.ok=true,.status=AuthStatus::Ok,.user=userInfo.value()};
        }
        if(result.status==storage::RepoStatus::AlreadyExists){
            continue;
        }
        }
        return AuthResult{.status=AuthStatus::Internal};
    }catch(const std::exception& e){
        return AuthResult{.status=AuthStatus::Internal,.message=e.what()};
    }

}
auth::AuthResult auth::AuthService::login(const std::string& accountId,const std::string& password){
    if(accountId.empty()||password.empty()){
        return AuthResult{.status=AuthStatus::InvalidArgument,.message="username or password is empty"};
    }
    //查询用户信息
    std::optional<storage::UserAuthInfo> result=userRepo_->findAuthInfoByAccountId(accountId);
    if(!result){
        return AuthResult{.status=AuthStatus::UserNotFound};
    }
    //校验stauts
    if(result->status!=0){
        return AuthResult{.status=AuthStatus::UserDisabled};
    }
    //校验登录密码
    if(!passwordHasher_.verifyPassword(password,result.value().passwordHash,result.value().passwordSalt)){
        return AuthResult{.status=AuthStatus::BadPassword,.message="password is wrong"};
    }
    auto userAuthInnfo=result.value();
    auto issueToken=tokenManager_.issueToken();
    auto createAtMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto lastSeenAtMs=createAtMs;
    storage::StoredUserSession session{.userId=userAuthInnfo.userId,.accountId=userAuthInnfo.accountId,.username=userAuthInnfo.username,.tokenHash=issueToken.tokenHash,.expireAtMs=issueToken.expireAtMs,.createAtMs=createAtMs,.lastSeenAtMs=lastSeenAtMs,.revoked=false};
    auto saveResult=userSessionRepo_->createSession(session);
    if(!saveResult.ok()){
        return AuthResult{.status=AuthStatus::Internal,.message=saveResult.message};
    }
    return AuthResult{.ok=true,.status=AuthStatus::Ok,.user=result.value(),.issuedToken=issueToken,.tokenExpireAtMs=issueToken.expireAtMs};
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
auth::AuthResult auth::AuthService::loginByToken(const std::string& rawToken){
    if(rawToken.empty()){
        return AuthResult{.status=AuthStatus::InvalidArgument,.message="rawToken is empty"};
    }
    //获取tokenHash
    auto tokenHash=tokenManager_.hashToken(rawToken);
    if(tokenHash.empty()){
        return AuthResult{.status=AuthStatus::InvalidToken,.message="Failed to get tokenHash"};
    }
    auto result=userSessionRepo_->findByTokenHash(tokenHash);
    if(!result){
        return AuthResult{.status=AuthStatus::InvalidToken};
    }
    //判断是否注销
    auto session=result.value();
    if(session.revoked){
        return AuthResult{.status=AuthStatus::TokenRevoked};
    }
    //更新最近使用时间
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(session.expireAtMs<=nowMs){
        return AuthResult{.status=AuthStatus::TokenExpired};
    }
    auto updateResult=userSessionRepo_->touchSession(session.tokenHash,nowMs);
    if(!updateResult.ok()){
        return AuthResult{.status=AuthStatus::Internal,.message="Failed to update lastSeenAtMs"};
    }
    storage::UserAuthInfo userInfo{.userId=session.userId,.accountId=session.accountId,.username=session.username};
    return AuthResult{.ok=true,.status=AuthStatus::Ok,.user=userInfo,.tokenExpireAtMs=session.expireAtMs};
    
}
auth::LogoutResult auth::AuthService::logout(const std::string& rawToken){
    if(rawToken.empty()){
        return LogoutResult{.status=AuthStatus::InvalidToken};
    }
    auto tokenHash=tokenManager_.hashToken(rawToken);
    if(tokenHash.empty()){
        return LogoutResult{.status=AuthStatus::InvalidToken};
    }
    auto findResult=userSessionRepo_->findByTokenHash(tokenHash);
    if(!findResult){
        return LogoutResult{.status=AuthStatus::InvalidToken};
    }
    if(findResult.value().revoked){//幂等注销
        return LogoutResult{.ok=true,.status=AuthStatus::Ok,.alreadyLoggedOut=true};
    }
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto result=userSessionRepo_->revokeSession(tokenHash,nowMs);
    if(result.ok()){
        return LogoutResult{.ok=true,.status=AuthStatus::Ok,.revokedNow=true};
    }
    return LogoutResult{.status=AuthStatus::Internal};
}