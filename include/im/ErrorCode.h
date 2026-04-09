#pragma once
#include <cstdint>

//错误码常量
enum class  ErrorCode: uint16_t{
    OK,
    BAD_JSON,//
    MISSING_FIELD,//缺少必要字段
    UNSUPPORTED_VER,//不支持的协议版本
    UNKNOWN_TYPE,//未知的消息类型
    NOT_AUTHED,//未认证
    INTERNAL//服务器内部错误

};
const char* errCodeToString(ErrorCode code){
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
        case ErrorCode::INTERNAL:
            return "Internal Server Error";
        default:
            return "Unknown Error Code";
    }

}