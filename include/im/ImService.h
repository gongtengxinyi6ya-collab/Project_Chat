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
#include "im/Group.h"
#include "im/GroupManager.h"
#include "im/SessionManager.h"
#include "config/ImConfig.h"
class TcpConnection;

/*唯一业务入口
*/
namespace im{
class Imservice{
public:
    using ConnKey=int;//连接标识
    using SendToConnKeyFn=std::function<bool (ConnKey,const std::string &payload)>;//回调通过Key由TcpServer代发

    explicit Imservice(uint32_t supportedVer=1,const ImConfig& config=ImConfig());
    void setSendToConnKey(SendToConnKeyFn fn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& payload);//唯一业务入口
    void onDisconnect(const std::shared_ptr<TcpConnection> & conn);//清理session和映射

    std::optional<Response> guardAuthenticated(const Request& ,const Session&);//登录门禁
    std::optional<Response> guardInGroup(const Request&,const Session&,const std::string&);//房间门禁

private:
    uint32_t supportedVer_{1};//支持版本，协议版本校验
    SendToConnKeyFn sendToConnKey_;
    SessionManager sessionManager_;
    uint64_t nextMsgId_{1};//全局递增消息id，用于推送消息唯一标识

    GroupManager groupManager_;//房间管理
    ImConfig imConfig_;//IM相关配置

    im::Response handleEcho(const im::Request& req,ConnKey key,Session& session);//回显
    im::Response handleAuth(const im::Request& req,ConnKey key,Session& session);//登录，把session状态改为Authed,绑定身份
    im::Response handleDm(const im::Request& req,ConnKey key,Session& session);//把私聊消息投递到目标连接，并回复发送方投递结果
    im::Response handleListUsers(const im::Request& req,ConnKey key,Session& session);//在线用户名列表
    uint64_t nowMs() const;//获取当前时间戳
    void decorate(im::Response& resp,std::optional<uint64_t> clentReqId=std::nullopt);//给任何响应/错误/推送加trace字段
    std::optional<std::string> usernameByKey(ConnKey key) const;//把connKey映射为username
    bool sendPush(ConnKey,Response,std::optional<uint64_t> clientReqid=std::nullopt);//统一对push做decorate,encode,sendToConnKey

    //群聊接口
    Response handleCreateGroup(const Request&,ConnKey,Session&);//创建群并加入群主，设置为当前活跃群
    size_t broadcastToGroup(const std::string&,const std::string&,ConnKey,const im::Response&push);//对房间内其他成员推送事件；
    im::Response handleJoin(const im::Request& req,ConnKey key,Session& session);//加入群
    im::Response handleLeave(const im::Request&,ConnKey,Session&);//退出群
    im::Response handleGroupMsg(const im::Request&,ConnKey,Session&);//提交房间消息
    im::Response handleGroupMembers(const im::Request&,ConnKey,Session&);//获取群聊成员列表
    Response handleListGroups(const Request&,ConnKey,Session&);//返回当前用户加入的群列表

    };
}
