#pragma once
#include <string>
#include <optional>
#include "storage/UserRepo.h"

namespace auth{
    enum class AuthStatus{
        Ok,
        InvalidArgument,
        UserNotFound,//用户不存在
        BadPassword,//密码禁用
        UserDisabled,//用户禁用
        Internal
    };
    struct AuthResult
    {
        bool ok{false};
        AuthStatus status;
        std::optional<storage::UserAuthInfo> user;
        std::string message{};
    };
}