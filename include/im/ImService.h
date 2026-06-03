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
#include "logger/LogContext.h"
#include "logger/LogLevel.h"
#include "storage/RepositoryBundle.h"
#include "storage/RepoResult.h"

class TcpConnection;

/*唯一业务入口
*/
namespace auth{
    class AuthService;
}
namespace im{
    class FriendService;//好友关系类向前声明
class Imservice{
public:
    class BroadcastResult{
        public:
        size_t sent{0};//成功发送数量
        size_t noSuchConnection{0};//没有连接数量
        size_t closed{0};//连接已关闭数量
        size_t overloaded{0};//输出缓冲区过载数量
        size_t dropped()const{//返回未成功发送数量，包括连接不存在、已关闭和过载的情况
            return noSuchConnection+closed+overloaded;
        }
    };
    //SendResult:区分连接存在，输出缓冲区过载，成功入队
    enum class SendResult{
        Ok,//成功入队
        NoSuchConnection,//连接不存在
        Closed,//连接关闭
        Overloaded//输出缓冲区过载
    };

    //账号级推送结果
    struct AccountPushResult{
        size_t sent{0};//成功推送的设备数量
        size_t noSuchConnection{0};//已失效的连接数量
        size_t closed{0};//已经关闭连接数量
        size_t overloaded{0};//满连接数量
        size_t failed()const{return noSuchConnection+closed+overloaded;}//返回失败设备总数
        bool delivered()const{return sent>0;}//是否至少投递到一个设备
    };

    using ConnKey=int;//连接标识
    using SendToConnKeyFn=std::function<SendResult (ConnKey,const std::string &payload)>;//回调通过Key由TcpServer代发

    explicit Imservice(uint32_t supportedVer=1,const ImConfig& config=ImConfig());
    ~Imservice();
    void setSendToConnKey(SendToConnKeyFn fn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& payload);//唯一业务入口
    void onDisconnect(const std::shared_ptr<TcpConnection> & conn);//清理session和映射

    std::optional<Response> guardAuthenticated(const Request& ,const Session&);//登录门禁
    std::optional<Response> guardInGroup(const Request&,const Session&,const std::string&);//房间门禁
    std::optional<Response> getStringField(const Request& req,const std::string&field,std::string&out,bool allowEmpty=false);//统一读取JSON字符型字段

    std::string_view sendResultToString(SendResult result) const;//发送结果转字符串，便于日志输出

    //持久化存储接口
    void setRepositories(storage::RepositoryBundle repos);//移动保存repos_
    bool hasRepositories()const;//
    void loadFromRepositories();//服务启动时从Repo恢复群和成员关系
private:
    uint32_t supportedVer_{1};//支持版本，协议版本校验
    SendToConnKeyFn sendToConnKey_;
    SessionManager sessionManager_;
    uint64_t nextMsgId_{1};//全局递增消息id，用于推送消息唯一标识

    GroupManager groupManager_;//房间管理
    ImConfig imConfig_;//IM相关配置

    im::Response handleEcho(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session);//回显
    im::Response handleAuth(const im::Request& req,ConnKey key,Session& session);//登录，把session状态改为Authed,绑定身份
    im::Response handleDm(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session);//把私聊消息投递到目标连接，并回复发送方投递结果
    im::Response handleListUsers(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session);//在线用户名列表
    uint64_t nowMs() const;//获取当前时间戳
    uint64_t nextMessageId();
    void decorate(im::Response& resp,std::optional<uint64_t> msgId=std::nullopt,std::optional<uint64_t> clientReqId=std::nullopt);//给任何响应/错误/推送加trace字段
    std::optional<std::string> usernameByKey(ConnKey key) const;//把connKey映射为username
    SendResult sendPush(ConnKey,const std::string &);//统一对push做decorate,encode,sendToConnKey
    std::optional<std::string> resolveTargetGroupId(const Request&,const Session&);//群id获取辅助方法
    //群聊接口
    Response handleCreateGroup(const Request&,[[maybe_unused]]ConnKey,Session&);//创建群并加入群主，设置为当前活跃群
    BroadcastResult broadcastToGroup(const std::string&,[[maybe_unused]]ConnKey,im::Response&push);//对房间内其他成员推送事件；
    im::Response handleJoin(const im::Request& req,ConnKey key,Session& session);//加入群
    im::Response handleLeave(const im::Request&,ConnKey,Session&);//退出群
    im::Response handleGroupMsg(const im::Request&,ConnKey,Session&);//提交房间消息
    im::Response handleGroupMembers(const im::Request&,[[maybe_unused]]ConnKey,Session&);//获取群聊成员列表
    Response handleListGroups(const Request&,[[maybe_unused]]ConnKey,Session&);//返回当前用户加入的群列表

    //日志上下文生成辅助方法
    LogContext makeReqCtx(ConnKey,const Request&,const Session&,const std::string& )const;//生成请求入口日志上下文
    LogContext makeRespCtx(ConnKey,const Request&,const Response&,const Session&,const std::string&)const;//生成响应出口日志上下文
    std::optional<std::string> tryExtractGroupId(const Request& req)const;
    std::optional<uint64_t> tryExtractMsgId(const Response& resp)const;

    //统一错误处理
    LogLevel mapErrorToLogLevel(im::ErrorCode code) const;//错误映射
    SendResult sendResponseWithLog(ConnKey key,const Request& req,Response& resp,const Session& session,const std::string& outEvet);//统一回包出口函数，处理日志，错误
    SendResult sendParseErrorWithLog( ConnKey key,Response& resp,const Session& session);//统一解析错误回包函数
    im::Response dispatcResqest(const Request&req,ConnKey key,Session& ssession);//分发并返回resp

    //持久化储存
    storage::RepositoryBundle repos_;//保存用户，群，消息三个Repo

    im::ErrorCode repoStatusToErrorCode(storage::RepoStatus status)const;//把存储层错误转换为IM协议错误码
    im::Response makeRepoError(const im::Request&req,storage::RepoStatus,const std::string&fallbackMsg)const;//把repo错误统一转换为Response
    im::Response handleGroupHistory(const Request& req,ConnKey key,Session& session);//获取群聊历史消息
    void saveOfflineForGroupMembers(const std::string& groupId,const std::string& fromUser,uint64_t msgId);//群消息发送后，为离线群成员记录离线索引
    im::Response handleOfflinelist(const Request& req,ConnKey key,Session& session);//客户端拉取自己的离线消息索引
    im::Response handleOfflineAck(const Request& req,ConnKey key,Session& session);//客户端确认离线消息已经处理，服务端删除离线索引

    //注册登录
    std::unique_ptr<auth::AuthService> authService_;//
    Response handleRegister(const Request& req,ConnKey key,Session& session);//处理客户端注册
    Response handleLogin(const Request& req,ConnKey key,Session& session);//处理密码登录

    //token登录和注销
    Response handleTokenLogin(const Request& req,ConnKey key,Session& session);//处理token登录
    Response handleLogout(const Request& req,ConnKey key,Session& session);//处理注销

    //用户资料接口
    Response handleGetProfile(const Request& req,ConnKey key,Session& session);//当前已登录用户查询自己的资料
    Response handleUpdateProfile(const Request& req,ConnKey key,Session& session);//当前登录用户修改自己的公开资料
    nlohmann::json buildMemberProfileList(const std::vector<std::string>&accountIds);//把GroupManager返回的accountId列表转换为客户端可展示的成员资料

    //好友相关接口
    std::unique_ptr<FriendService> friendService_;
    Response handleSearchUser(const Request& req,ConnKey key,Session& session);//提交好友搜索请求
    Response handleListFriends(const Request& req,ConnKey key,Session& session);//获取好友列表
    AccountPushResult pushToAccount(const std::string&targetAccount,im::Response&push);//将一条消息推送给目标账号当前在线的全部设备
    AccountPushResult notifyFriendEvent(const std::string&targetAccountId,const std::string&event,nlohmann::json data);//统一发送好友模块事件，避免在多个handler中重复拼装推送消息
    //好友请求接口
    Response handleSendFriendRequest(const Request& req,ConnKey key,Session& session);//提交好友申请
    Response handleListFriendRequests(const Request& req,ConnKey key,Session& session);//请求当前账号收到的待处理申请
    Response handleAcceptFriendRequest(const Request& req,ConnKey key,Session& session);//同意申请并创建双向关系
    Response handleRejectFriendRequest(const Request& req,ConnKey key,Session& session);//拒绝好友申请
};
}