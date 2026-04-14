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
    LIST_USERS_REQ=8,//用户列表请求
    LIST_USERS_RESP=9,//用户列表响应
    JOIN_REQ=10,
    JOIN_RESP,
    LEAVE_REQ,
    LEAVE_RESP,
    ROOM_MSG_REQ,
    ROOM_MSG_RESP,
    ROOM_MSG_PUSH,
    ROOM_MEMBERS_REQ,
    ROOM_MEMBERS_RESP,
    ROOM_EVENT_PUSH,
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
        case 8:
            return MsgType::LIST_USERS_REQ;
        case 9:
            return MsgType::LIST_USERS_RESP;
        case 10:
            return MsgType::JOIN_REQ;
        case 11:
            return MsgType::JOIN_RESP;
        case 12:
            return MsgType::LEAVE_REQ;
        case 13:
            return MsgType::LEAVE_RESP;
        case 14:
            return MsgType::ROOM_MSG_REQ;
        case 15:
            return MsgType::ROOM_MSG_RESP;
        case 16:
            return MsgType::ROOM_MSG_PUSH;
        case 17:
            return MsgType::ROOM_MEMBERS_REQ;
        case 18:
            return MsgType::ROOM_MEMBERS_RESP;
        case 19:
            return MsgType::ROOM_EVENT_PUSH;
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