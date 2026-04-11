#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <functional>

#include "im/MsgType.h"
#include "im/ErrorCode.h"
#include "im/Session.h"
#include "im/ImMessage.h"
#include "im/ImCodec.h"

class TcpConnection;

/*唯一业务入口
*/
namespace im{
class Imservice{
public:
    using ConnKey=int;//连接标识
    using SendToConnKeyFn=std::function<void (ConnKey,const std::string &payload)>;//回调通过Key由TcpServer代发

    explicit Imservice(uint32_t supportedVer=1);
    void setSendToConnKey(SendToConnKeyFn fn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& payload);//唯一业务入口
    void onDisconnect(const std::shared_ptr<TcpConnection> & conn);//清理session和映射
    Session& getOrCreateSession(ConnKey key);//不存在则创建,保证每个连接都有Session
    std::optional<im::Response> guarddAuthed(const im::Request& req,const Session& session);//统一门禁
    void cleanupUserConn(ConnKey key,const Session& session);
private:
    uint32_t supportedVer_{1};//支持版本，协议版本校验
    SendToConnKeyFn sendToConnKey_;
    std::unordered_map<ConnKey,Session> sessions_;//每条连接一份状态
    std::unordered_map<std::string,ConnKey> userConnMap_;//用户id到连接的映射，私聊定位
    
    im::Response handleEcho(const im::Request& req,ConnKey key,Session& session);//回显
    im::Response handleAuth(const im::Request& req,ConnKey key,Session& session);//登录，把session状态改为Authed,绑定身份
    im::Response handleDm(const im::Request& req,ConnKey key,Session& session);//把私聊消息投递到目标连接，并回复发送方投递结果
    im::Response handleListUsers(const im::Request& req,ConnKey key,Session& session);//在线用户名列表
};
}
