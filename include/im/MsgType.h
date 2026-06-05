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
    OFFLINE_LIST_REQ,//离线消息列表请求
    OFFLINE_LIST_RESP,//离线消息列表响应
    OFFLINE_ACK_REQ,//离线消息确认请求
    OFFLINE_ACK_RESP,//离线消息确认响应
    REGISTER_REQ,//客户端登录请求
    REGISTER_RESP,//注册响应
    LOGIN_REQ,//客户端登录请求
    LOGIN_RESP,//客户端登录响应
    TOKEN_LOGIN_REQ,//
    TOKEN_LOGIN_RESP,
    LOGOUT_REQ,
    LOGOUT_RESP,
    GET_PROFILE_REQ,//获取自己资料
    GET_PROFILE_RESP,//返回自己的资料
    UPDATE_PROFILE_REQ,//修改资料请求
    UPDATE_PROFILE_RESP,//修改资料响应
    SEARCH_USER_REQ,//搜索好友请求
    SEARCH_USER_RESP,//搜索好友响应
    LIST_FRIENDS_REQ,//列出好友列表请求
    LIST_FRIENDS_RESP,
    SEND_FRIEND_REQUEST_REQ,//发送好友申请请求
    SEND_FRIEND_REQUEST_RESP,
    LIST_FRIEND_REQUEST_REQ,//列出好友申请请求
    LIST_FRIEND_REQUEST_RESP,
    ACCEPT_FRIEND_REQUEST_REQ,//接收好友申请
    ACCEPT_FRIEND_REQUEST_RESP,
    REJECT_FRIEND_REQUEST_REQ,//拒绝好友申请
    REJECT_FRIEND_REQUEST_RESP,
    REMOVE_FRIEND_REQ,//删除好友请求
    REMOVE_FRIEND_RESP,
    FRIEND_EVENT_PUSH,//好友事件推送，如被添加，被删除等
    DM_HISTORY_REQ,//私聊历史消息请求
    DM_HISTORY_RESP,
    CONVERSATION_LIST_REQ,
    CONVERSATION_LIST_RESP,
    CONVERSATION_READ_REQ,
    CONVERSATION_READ_RESP,
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
        case 30:
            return MsgType::REGISTER_REQ;
        case 31:
            return MsgType::REGISTER_RESP;
        case 32:
            return MsgType::LOGIN_REQ;
        case 33:
            return MsgType::LOGIN_RESP;
        case 34:
            return MsgType::TOKEN_LOGIN_REQ;
        case 35:
            return MsgType::TOKEN_LOGIN_RESP;
        case 36:
            return MsgType::LOGOUT_REQ;
        case 37:
            return MsgType::LOGOUT_RESP;
        case 38:
            return MsgType::GET_PROFILE_REQ;
        case 39:
            return MsgType::GET_PROFILE_RESP;
        case 40:
            return MsgType::UPDATE_PROFILE_REQ;
        case 41:
            return MsgType::UPDATE_PROFILE_RESP;
        case 42:
            return MsgType::SEARCH_USER_REQ;
        case 43:
            return MsgType::SEARCH_USER_RESP;
        case 44:            
            return MsgType::LIST_FRIENDS_REQ;
        case 45:            
            return MsgType::LIST_FRIENDS_RESP;
        case 46:
            return MsgType::SEND_FRIEND_REQUEST_REQ;
        case 47:
            return MsgType::SEND_FRIEND_REQUEST_RESP;
        case 48:
            return MsgType::LIST_FRIEND_REQUEST_REQ;   
        case 49:
            return MsgType::LIST_FRIEND_REQUEST_RESP;
        case 50:
            return MsgType::ACCEPT_FRIEND_REQUEST_REQ;
        case 51:
            return MsgType::ACCEPT_FRIEND_REQUEST_RESP;
        case 52:
            return MsgType::REJECT_FRIEND_REQUEST_REQ;
        case 53:
            return MsgType::REJECT_FRIEND_REQUEST_RESP;
        case 54:
            return MsgType::REMOVE_FRIEND_REQ;
        case 55:
            return MsgType::REMOVE_FRIEND_RESP;
        case 56:
            return MsgType::FRIEND_EVENT_PUSH;
        case 57:
            return MsgType::DM_HISTORY_REQ;
        case 58:
            return MsgType::DM_HISTORY_RESP;
        case 59:
            return MsgType::CONVERSATION_LIST_REQ;
        case 60:
            return MsgType::CONVERSATION_LIST_RESP;
        case 61:
            return MsgType::CONVERSATION_READ_REQ;
        case 62:
            return MsgType::CONVERSATION_READ_RESP;
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