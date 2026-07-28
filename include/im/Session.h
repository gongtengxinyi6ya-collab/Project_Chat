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

enum class AuthOperation : std::uint8_t {//认证操作
    None,
    Register,
    PasswordLogin,
    TokenLogin,
    Logout
};
struct Session{
    ConnState state_;
    std::string username_;
    std::string accountId_;//账号唯一标识
    std::unordered_set<std::string> joinedGroupIds_;//已经加入的群聊集合
    uint64_t userId_{0};//数据库稳定主键
    std::string peerIp_{};
    uint16_t peerPort_{0};
    AuthOperation pendingAuthOperation_{AuthOperation::None};//防止同一连接提交两个操作
    std::uint64_t authOperationId_{0};//识别异步完成结果是否属于当前操作
    
    Session():state_(ConnState::Connected),username_(""){}
};
}