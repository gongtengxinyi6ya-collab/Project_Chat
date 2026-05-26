#pragma once
#include <cstdint>

//错误码常量
namespace im {

enum class ErrorCode: uint16_t{
    OK,//成功
    BAD_JSON,//JSON格式错误
    MISSING_FIELD,//缺少必要字段
    UNSUPPORTED_VER,//不支持的协议版本
    UNKNOWN_TYPE,//未知的消息类型
    NOT_AUTHED,//未登录
    USER_EXISTS,//重复注册
    BAD_REQUEST,//字段类型不对
    NO_SUCH_USER,//用户不存在
    NO_SUCH_GROUP,//群不存在
    ALREADY_IN_GROUP,//已在群
    NOT_IN_GROUP,//不在群内
    GROUP_NAME_INVALID,//群名过长或非法
    USER_NOT_FOUND,//登录时用户不存在
    BAD_PASSWORD,//密码错误
    WEAK_PASSWORD,//密码太短或格式不合法
    TOKEN_INVALID,//token不存在或格式错误
    TOKEN_EXPIRED,//token过期
    TOKEN_REVOKED,//用户主动退出登录后再次使用token
    INTERNAL//服务器内部错误

};
//把错误码转换为字符串，便于调试和日志记录
inline const char* errCodeToString(ErrorCode code){
    switch(code){
        case ErrorCode::OK:
            return "OK";
        case ErrorCode::BAD_JSON:
            return "Bad JSON";
        case ErrorCode::MISSING_FIELD:
            return "Missing Field";
        case ErrorCode::UNSUPPORTED_VER:
            return "Unsupported Version";
        case ErrorCode::UNKNOWN_TYPE:
            return "Unknown Type";
        case ErrorCode::NOT_AUTHED:
            return "Not Authenticated";
        case ErrorCode::USER_EXISTS:
            return "User exists";
        case ErrorCode::BAD_REQUEST:
            return "Request Error";
        case ErrorCode::NO_SUCH_USER:
            return "User do not exist";
        case ErrorCode::NO_SUCH_GROUP:
            return "Group do not exist";
        case ErrorCode::ALREADY_IN_GROUP:
            return "User is already in the group";
        case ErrorCode::NOT_IN_GROUP:
            return "The user is not in the group";
        case ErrorCode::GROUP_NAME_INVALID:
            return "Group name is invalid";
        case ErrorCode::USER_NOT_FOUND:
            return "User is not exist";
        case ErrorCode::BAD_PASSWORD:
            return "Password is wrong";
        case ErrorCode::WEAK_PASSWORD:
            return "Password is too weak";
        case ErrorCode::INTERNAL:
            return "Internal Server Error";
        case ErrorCode::TOKEN_INVALID:
            return "Token is invalid";
        case ErrorCode::TOKEN_EXPIRED:
            return "Token is expired";
        case ErrorCode::TOKEN_REVOKED:
            return "Token is revoked";
        default:
            return "Unknown Error Code";
    }
}

}