#pragma once
#include <cstdint>
#include <optional>
enum class MsgType:uint16_t{
    AUTH_REQ=1,
    AUTH_RESP=2,
    ECHO_REQ=3,
    ECHO_RESP=4,
    ERR=255
};
//从整数转换为MsgType枚举
std::optional<MsgType> msgTypeFromInt(uint32_t v){
    switch(v){
        case 1:
            return MsgType::AUTH_REQ;
        case 2:
            return MsgType::AUTH_RESP;
        case 3:
            return MsgType::ECHO_REQ;
        case 4:
            return MsgType::ECHO_RESP;
        case 255:
            return MsgType::ERR;
        default:
            return std::nullopt;//无效的消息类型
    }
}
uint32_t msgTypeToInt(MsgType t){
    return static_cast<uint32_t>(t);
}
bool isValidMsgType(uint32_t v){
    return msgTypeFromInt(v).has_value();
}