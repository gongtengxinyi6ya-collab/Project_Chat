#include <string>
#include <cstdint>

/*
引入Session和状态机，每个连接对应一个Session对象，保存认证状态和用户信息
*/
enum class ConnState{
    Connected,//已连接但未认证
    Authed,//已认证
    Closing//正在关闭
};
struct Session{
    ConnState stete_;
    std::string username_;
    Session():stete_(ConnState::Connected),username_(""){}
};