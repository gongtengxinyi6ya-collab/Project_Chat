#pragma once
#include <cstdint>

//错误码常量
namespace im {

enum class ErrorCode: uint16_t{
    OK,//成功
    BAD_JSON,//
    MISSING_FIELD,//缺少必要字段
    UNSUPPORTED_VER,//不支持的协议版本
    UNKNOWN_TYPE,//未知的消息类型
    NOT_AUTHED,//未认证
    USER_EXISTS,//用户已存在
    BAD_REQUEST,//字段类型不对
    NO_SUCH_USER,//
    NOT_IN_ROOM,//不在房间
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
        case ErrorCode::NOT_IN_ROOM:
            return "User is not in room";
        case ErrorCode::INTERNAL:
            return "Internal Server Error";
        default:
            return "Unknown Error Code";
    }
}

}