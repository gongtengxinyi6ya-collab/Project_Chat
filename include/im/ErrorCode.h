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
    PROFILE_NOT_FOUND,//当前账号没有对应资料记录
    NICKNAME_INVALID,//昵称为空或超长
    SIGNATURE_TOO_LONG,//签名长度超过限制
    AVATAR_URL_TOO_LONG,//头像URL长度超过限制
    CANNOT_ADD_SELF,//不能添加自己为好友
    ALREADY_FRIENDS,//已经是好友关系
    FRIEND_REQUEST_EXISTS,//已经有未处理的好友申请
    FRIEND_REQUEST_NOT_FOUND,//没有找到好友申请
    FRIEND_REQUEST_ALREADY_HANDLED,//好友申请已经被处理
    FRIEND_REQUEST_FORBIDDEN,//没有权限处理好友申请
    NOT_FRIENDS,//不存在有效好友关系
    RECIPIENT_OFFLINE,//接收方不在线
    DELIVERY_OVERLOADED,//消息发送过快导致服务器发送队列积压
    INVALID_ACK_PAYLOAD,//msgIds/offlineMsgIds格式错误
    ACK_BATCH_TOO_LARGE,//ACK数组超过限制
    MESSAGE_NOT_FOUND,//消息不存在
    MESSAGE_ACK_FORBIDDEN,//ACK消息无权确认
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
        case ErrorCode::PROFILE_NOT_FOUND:
            return "Profile not found";
        case ErrorCode::NICKNAME_INVALID:
            return "Nickname is invalid";
        case ErrorCode::SIGNATURE_TOO_LONG:
            return "Signature is too long";
        case ErrorCode::AVATAR_URL_TOO_LONG:
            return "Avatar URL is too long";
        case ErrorCode::CANNOT_ADD_SELF:
            return "Cannot add self as friend";
        case ErrorCode::ALREADY_FRIENDS:
            return "Already friends";
        case ErrorCode::FRIEND_REQUEST_EXISTS:
            return "Friend request already exists";
        case ErrorCode::FRIEND_REQUEST_NOT_FOUND:
            return "Friend request not found";
        case ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED:
            return "Friend request already handled";
        case ErrorCode::FRIEND_REQUEST_FORBIDDEN:
            return "Friend request forbidden";
        case ErrorCode::NOT_FRIENDS:
            return "Not friends";
        case ErrorCode::RECIPIENT_OFFLINE:
            return "Recipient is offline";
        case ErrorCode::DELIVERY_OVERLOADED:
            return "Server is overloaded, try again later";
        case ErrorCode::INVALID_ACK_PAYLOAD:
            return "Invalid ACK payload";
        case ErrorCode::ACK_BATCH_TOO_LARGE:
            return "ACK batch size is too large";   
        case ErrorCode::MESSAGE_NOT_FOUND:
            return "Message not found";
        case ErrorCode::MESSAGE_ACK_FORBIDDEN:
            return "You are not allowed to ACK this message";
        default:
            return "Unknown Error Code";
    }
}

}