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

    CREATE_GROUP_REQ,//创建群聊请求
    CREATE_GROUP_RESP,
    JOIN_GROUP_REQ,
    JOIN_GROUP_RESP,
    LEAVE_GROUP_REQ,
    LEAVE_GROUP_RESP,
    GROUP_MEMBERS_REQ,
    GROUP_MEMBERS_RESP,
    LIST_GROUPS_REQ,
    LIST_GROUPS_RESP,
    GROUP_EVENT_PUSH,
    GROUP_MSG_REQ,
    GROUP_MSG_RESP,
    GROUP_MSG_PUSH,
    GROUP_HISTORY_REQ,
    GROUP_HISTORY_RESP,
    OFFLINE_LIST_REQ,
    OFFLINE_LIST_RESP,
    OFFLINE_ACK_REQ,
    OFFLINE_ACK_RESP,
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
            return MsgType::CREATE_GROUP_REQ;
        case 11:
            return MsgType::CREATE_GROUP_RESP;
        case 12:
            return MsgType::JOIN_GROUP_REQ;
        case 13:
            return MsgType::JOIN_GROUP_RESP;
        case 14:
            return MsgType::LEAVE_GROUP_REQ;
        case 15:
            return MsgType::LEAVE_GROUP_RESP;
        case 16:
            return MsgType::GROUP_MEMBERS_REQ;
        case 17:
            return MsgType::GROUP_MEMBERS_RESP;
        case 18:
            return MsgType::LIST_GROUPS_REQ;
        case 19:
            return MsgType::LIST_GROUPS_RESP;
        case 20:
            return MsgType::GROUP_EVENT_PUSH;
        case 21:
            return MsgType::GROUP_MSG_REQ;
        case 22:
            return MsgType::GROUP_MSG_RESP;
        case 23:
            return MsgType::GROUP_MSG_PUSH;
        case 24:
            return MsgType::GROUP_HISTORY_REQ;
        case 25:
            return MsgType::GROUP_HISTORY_RESP;
        case 26:
            return MsgType::OFFLINE_LIST_REQ;
        case 27:
            return MsgType::OFFLINE_LIST_RESP;
        case 28:
            return MsgType::OFFLINE_ACK_REQ;
        case 29:
            return MsgType::OFFLINE_ACK_RESP;
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