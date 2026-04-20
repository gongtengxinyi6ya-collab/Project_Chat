#pragma once
#include <string>
#include <cstdint>
#include <unordered_set>

/*
引入Session和状态机，每个连接对应一个Session对象，保存认证状态和用户信息
*/
namespace im{
enum class ConnState{
    Connected,//已连接但未认证
    Authed,//已认证
    Closing//正在关闭
};
struct Session{
    ConnState state_;
    std::string username_;
    std::unordered_set<std::string> joinedGroupIds_;//已经加入的群聊集合

    
    Session():state_(ConnState::Connected),username_(""){}
};
}