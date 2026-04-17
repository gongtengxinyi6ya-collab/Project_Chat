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
#include "im/RoomManager.h"
#include "im/SessionManager.h"
class TcpConnection;

/*唯一业务入口
*/
namespace im{
class Imservice{
public:
    using ConnKey=int;//连接标识
    using SendToConnKeyFn=std::function<bool (ConnKey,const std::string &payload)>;//回调通过Key由TcpServer代发

    explicit Imservice(uint32_t supportedVer=1);
    void setSendToConnKey(SendToConnKeyFn fn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& payload);//唯一业务入口
    void onDisconnect(const std::shared_ptr<TcpConnection> & conn);//清理session和映射

    std::optional<Response> guardAuthenticated(const Request& ,const Session&);//登录门禁
    std::optional<Response> guardInRoom(const Request&,const Session&);//房间门禁

private:
    uint32_t supportedVer_{1};//支持版本，协议版本校验
    SendToConnKeyFn sendToConnKey_;
    SessionManager sessionManager_;
    uint64_t nextMsgId_{1};//全局递增消息id，用于推送消息唯一标识

    RoomManager roomManager_;//房间管理

    im::Response handleEcho(const im::Request& req,ConnKey key,Session& session);//回显
    im::Response handleAuth(const im::Request& req,ConnKey key,Session& session);//登录，把session状态改为Authed,绑定身份
    im::Response handleDm(const im::Request& req,ConnKey key,Session& session);//把私聊消息投递到目标连接，并回复发送方投递结果
    im::Response handleListUsers(const im::Request& req,ConnKey key,Session& session);//在线用户名列表
    uint64_t nowMs() const;//获取当前时间戳
    void decorate(im::Response& resp,std::optional<uint64_t> clentReqId=std::nullopt);//给任何响应/错误/推送加trace字段
    std::optional<std::string> usernameByKey(ConnKey key) const;//把connKey映射为username

    //房间接口
    
    void removeFromRoom(ConnKey,Session& session);//退房清理（断连/换房/leave)
    void broadcastToRoom(const std::string&,ConnKey,const im::Response&push);//对房间内其他成员推送事件；
    im::Response handleJoin(const im::Request& req,ConnKey key,Session& session);//加入房间
    im::Response handleLeave(const im::Request&,ConnKey,Session&);//退出房间
    im::Response handleRoomMsg(const im::Request&,ConnKey,Session&);//提交房间消息
    im::Response handleRoomMembers(const im::Request&,ConnKey,Session&);//获取房间成员列表
    

    };
}
