#pragma once
#include <cstdint>
#include <optional>
namespace im{
enum class MsgType:uint16_t{
    AUTH_REQ=1,//
    AUTH_RESP=2,//登录响应
    ECHO_REQ=3,//
    ECHO_RESP=4,
    DM_REQ=5,//私聊请求
    DM_RESP=6,//私聊响应
    DM_PUSH=7,//私聊消息推送
    ERR=255
};
//从整数转换为MsgType枚举
inline std::optional<MsgType> msgTypeFromInt(uint32_t v){
    switch(v){
        case 1:
            return MsgType::AUTH_REQ;
        case 2:
            return MsgType::AUTH_RESP;
        case 3:
            return MsgType::ECHO_REQ;
        case 4:
            return MsgType::ECHO_RESP;
        case 5:
            return MsgType::DM_REQ;
        case 6:
            return MsgType::DM_RESP;
        case 7:
            return MsgType::DM_PUSH;
        case 255:
            return MsgType::ERR;
        default:
            return std::nullopt;//无效的消息类型
    }
}
inline uint32_t msgTypeToInt(MsgType t){
    return static_cast<uint32_t>(t);
}
inline bool isValidMsgType(uint32_t v){
    return msgTypeFromInt(v).has_value();
}
}