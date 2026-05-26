#pragma once
#include <string>
#include <optional>
#include "storage/UserRepo.h"
#include "security/TokenManager.h"
namespace auth{
    enum class AuthStatus{
        Ok,
        InvalidArgument,
        UserNotFound,//用户不存在
        BadPassword,//密码禁用
        UserDisabled,//用户禁用
        AlreadyExist,//已经存在
        WeakPassword,//密码强度太弱
        InvalidToken,//token不存在
        TokenExpired,//token过期
        TokenRevoked,//token吊销
        Internal
    };
    struct AuthResult
    {
        bool ok{false};
        AuthStatus status{AuthStatus::Internal};
        std::optional<storage::UserAuthInfo> user{std::nullopt};
        std::string message{};
        std::optional<security::IssuedToken> issuedToken{std::nullopt};//密码登录成功时，把新签发的token返回给Imservice
    };
}