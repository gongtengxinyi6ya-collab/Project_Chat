#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <functional>
#include <atomic>
#include <vector>

#include "im/MsgType.h"
#include "im/ErrorCode.h"
#include "im/Session.h"
#include "im/ImMessage.h"
#include "im/ImCodec.h"
#include "im/Group.h"
#include "im/GroupManager.h"
#include "im/SessionManager.h"
#include "config/ImConfig.h"
#include "config/IdConfig.h"
#include "logger/LogContext.h"
#include "logger/LogLevel.h"
#include "storage/RepositoryBundle.h"
#include "storage/RepoResult.h"
#include "id/SnowflakeGenerator.h"
#include "security/rate_limit/RateLimitKeyType.h"
#include "im/GroupMessagePersistenceTypes.h"
#include "im/DirectMessagePersistenceService.h"
#include "im/DirectMessagePersistenceTypes.h"
#include "im/AsyncDbResult.h"
#include "infra/thread/ThreadTypes.h"
#include "im/ConversationTypes.h"
#include "net/SendTypes.h"
#include "im/RedisLimitTypes.h"

#include "im/PendingContextTypes.h"
class TcpConnection;

/*唯一业务入口
*/
namespace auth{
    class AuthService;
}
namespace security{
    class RateLimiter;
}


namespace im{
    class FriendService;//好友关系类向前声明
    class ConversationService;//会话管理
    class MessageSyncService;//消息同步服务
    class MessageAckService;//消息确认服务
    class GroupService;//群管理服务
    class GroupJoinService;//管理入群申请
    class GroupMessagePersistenceService;//消息一致性服务
    class DirectMessagePersistenceService;
class Imservice{
public:
    
    //账号级推送结果
    struct AccountPushResult{
        size_t sent{0};//成功推送的设备数量
        size_t noSuchConnection{0};//已失效的连接数量
        size_t closed{0};//已经关闭连接数量
        size_t overloaded{0};//满连接数量
        size_t internalFailed{0};//失败数量
        size_t failed()const{return noSuchConnection+closed+overloaded+internalFailed;}//返回失败设备总数
        bool delivered()const{return sent>0;}//是否至少投递到一个设备
    };
    //回调类型
    using ConnKey=net::ConnKey;//连接标识
    using SendResult=net::SendResult;
    
    using MessageTask = std::function<void()>;//异步任务接口
    using SubmitMessageTaskFn =std::function<infra::thread::TaskSubmitResult(const std::string& orderingKey,MessageTask)>;//提交消息持久化任务到工作线程池
    using PostToBaseLoopFn =std::function<bool(MessageTask)>;//投递持久化的结果任务到baseLoop线程
    using BatchSendFn = std::function<net::BatchSendResult(const std::vector<ConnKey>&, net::SharedPayload)>;
    using SubmitDbTaskFn =std::function<infra::thread::TaskSubmitResult(MessageTask)>;//投递数据库查询任务
    using SubmitRedisTaskFn =std::function<infra::thread::TaskSubmitResult(const std::string& orderingKey,MessageTask task)>;
    using RateLimitCompletion =std::function<void(AsyncRateLimitResult)>;
    using SubmitAuthTaskFn =std::function<infra::thread::TaskSubmitResult(const std::string& orderingKey,std::function<void()> task)>;

    explicit Imservice(uint32_t supportedVer=1,const ImConfig& config=ImConfig(),const IdConfig& idconfig=IdConfig());
    ~Imservice();

    //回调设置
    void setMessageAsyncExecutor(SubmitMessageTaskFn submitFn);//注入群消息异步处理所需的执行器
    void setBaseLoopPoster(PostToBaseLoopFn postFn);//工作线程回投baseLoop
    void stopAcceptingAsyncMessages();//服务关闭时禁止再接受新的异步消息任务
    void setBatchSender(BatchSendFn fn);
    void setDbReadExecutor(SubmitDbTaskFn submitFn);
    void setRedisAsyncExecutor(SubmitRedisTaskFn submitFn,bool failOpen);
    void setAuthAsyncExecutor(SubmitAuthTaskFn submitFn);

    void onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& payload);//唯一业务入口
    void onDisconnect(const std::shared_ptr<TcpConnection> & conn);//清理session和映射

    void shutdown();
    
    std::optional<Response> guardAuthenticated(const Request& ,const Session&);//登录门禁
    std::optional<Response> guardInGroup(const Request&,const Session&,const std::string&);//房间门禁
    std::optional<Response> getStringField(const Request& req,const std::string&field,std::string&out,bool allowEmpty=false);//统一读取JSON字符型字段

    std::string_view sendResultToString(SendResult result) const;//发送结果转字符串，便于日志输出

    //持久化存储接口
    void setRepositories(storage::RepositoryBundle repos);//移动保存repos_
    bool hasRepositories()const;//
    void loadFromRepositories();//服务启动时从Repo恢复群和成员关系

    void setRateLimiter(std::shared_ptr<security::RateLimiter> limiter);
private:
    uint32_t supportedVer_{1};//支持版本，协议版本校验

    SessionManager sessionManager_;
    
    GroupManager groupManager_;//房间管理
    ImConfig imConfig_;//IM相关配置
    IdConfig idConfig_;//id配置
    snowflakeId::SnowflakeIdGenerator idGenerator_;

    Response handleEcho(const Request& req,[[maybe_unused]]ConnKey key,Session& session);//回显
    Response handleAuth(const Request& req,ConnKey key,Session& session);//登录，把session状态改为Authed,绑定身份
    Response handleListUsers(const Request& req,[[maybe_unused]]ConnKey key,Session& session);//在线用户名列表
    uint64_t nowMs() const;//获取当前时间戳
    uint64_t nextMessageId();
    void decorate(Response& resp,std::optional<uint64_t> msgId=std::nullopt,std::optional<uint64_t> clientReqId=std::nullopt);//给任何响应/错误/推送加trace字段

    //群聊
    std::unique_ptr<GroupService> groupService_;
    Response handleCreateGroup(const Request&,[[maybe_unused]]ConnKey,Session&);//创建群并加入群主，设置为当前活跃群
    net::BatchSendResult broadcastToGroup(const std::string&,[[maybe_unused]]ConnKey,Response&push);//对房间内其他成员推送事件；
    Response handleJoin(const Request& req,ConnKey key,Session& session);//加入群
    Response handleLeave(const Request&,ConnKey,Session&);//退出群

    Response handleGroupMembers(const Request&,[[maybe_unused]]ConnKey,Session&);//获取群聊成员列表
    Response handleListGroups(const Request&,[[maybe_unused]]ConnKey,Session&);//返回当前用户加入的群列表
    Response handleKickGroupMember(const Request& req, ConnKey key, Session& session);//踢出群成员
    Response handleSetGroupAdmin(const Request& req, ConnKey key, Session& session);//设置群管理员
    Response handleTransferGroupOwner(const Request& req, ConnKey key, Session& session);//转让群主
    Response handleInviteGroupMember(const Request& req,ConnKey key,Session& session);
    Response handleDissolveGroup(const Request& req,ConnKey key,Session& session);

    //入群申请
    std::unique_ptr<GroupJoinService> groupJoinService_;
    Response handleApplyGroupJoin(const Request& req, ConnKey key, Session& session);//处理入群申请
    Response handleListGroupJoinRequest(const Request& req,ConnKey key,Session& session);//获取入群申请列表
    Response handleSearchGroups(const Request& req,ConnKey key,Session& session);//查询群
    Response handleReviewGroupJoin(const Request& req,ConnKey key,Session& session);//查询群
    
    //日志上下文生成辅助方法
    LogContext makeReqCtx(ConnKey,const Request&,const Session&,const std::string& )const;//生成请求入口日志上下文
    LogContext makeRespCtx(ConnKey,const Request&,const Response&,const Session&,const std::string&)const;//生成响应出口日志上下文
    std::optional<std::string> tryExtractGroupId(const Request& req)const;
    std::optional<uint64_t> tryExtractMsgId(const Response& resp)const;

    //统一错误处理
    LogLevel mapErrorToLogLevel(ErrorCode code) const;//错误映射
    SendResult sendResponseWithLog(ConnKey key,const Request& req,Response& resp,const Session& session,const std::string& outEvet);//统一回包出口函数，处理日志，错误
    SendResult sendParseErrorWithLog( ConnKey key,Response& resp,const Session& session);//统一解析错误回包函数
    DispatchResult dispatchRequest(const Request&req,ConnKey key,Session& ssession,const std::shared_ptr<TcpConnection>& connection);//分发并返回resp

    //持久化储存
    storage::RepositoryBundle repos_;//保存用户，群，消息三个Repo

    ErrorCode repoStatusToErrorCode(storage::RepoStatus status)const;//把存储层错误转换为IM协议错误码
    Response makeRepoError(const Request&req,storage::RepoStatus,const std::string&fallbackMsg)const;//把repo错误统一转换为Response
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
    nlohmann::json buildMemberProfileList(const std::string& groupId);//把GroupManager返回的accountId列表转换为客户端可展示的成员资料

    //好友相关接口
    std::shared_ptr<FriendService> friendService_;
    Response handleSearchUser(const Request& req,ConnKey key,Session& session);//提交好友搜索请求
    Response handleListFriends(const Request& req,ConnKey key,Session& session);//获取好友列表
    Response handleRemoveFriend(const Request& req,ConnKey key,Session& session);//删除好友
    AccountPushResult pushToAccount(const std::string&targetAccount,Response&push);//将一条消息推送给目标账号当前在线的全部设备
    AccountPushResult notifyFriendEvent(const std::string&targetAccountId,const std::string&event,nlohmann::json data);//统一发送好友模块事件，避免在多个handler中重复拼装推送消息

    //好友请求接口
    Response handleSendFriendRequest(const Request& req,ConnKey key,Session& session);//提交好友申请
    Response handleListFriendRequests(const Request& req,ConnKey key,Session& session);//请求当前账号收到的待处理申请
    Response handleAcceptFriendRequest(const Request& req,ConnKey key,Session& session);//同意申请并创建双向关系
    Response handleRejectFriendRequest(const Request& req,ConnKey key,Session& session);//拒绝好友申请

    //会话列表展示
    std::shared_ptr<ConversationService> conversationService_;

    //消息同步服务
    std::shared_ptr<MessageSyncService> messageSyncService_;

    //消息确认服务
    std::shared_ptr<MessageAckService> messageAckService_;

    //业务限流服务
    std::shared_ptr<security::RateLimiter> rateLimiter_;//处理关键请求前进行频率限制
    Response makeRateLimitError(const Request& req,const security::RateLimitResult& result);//将限流结果转换为错误响应
    std::optional<Response> checkRateLimitOrError(const Request& req,const security::RateLimitResult& result);//辅助方法
    SubmitRedisTaskFn submitRedisTask_;//回调提交redis任务
    bool redisFailOpen_{true};
    infra::thread::TaskSubmitResult submitRateLimitCheck(RateLimitAction action,const std::string& subject,RateLimitCompletion completion);//构造异步限流任务，并提交到redis线程池
    std::optional<Response> evaluateRateLimitResult(const Request& request,const AsyncRateLimitResult& result);//根据异步redis结果，判断是否立即返回响应
    Session* resolvePendingSender(const std::weak_ptr<TcpConnection>& connection,ConnKey key,const std::string& accountId);//异步限流完成后重新确定连接和session有效
    void sendDeferredSubmitFailure(const std::weak_ptr<TcpConnection>& connection,ConnKey key,const std::string& accountId,const Request& request,infra::thread::TaskSubmitResult submitResult,std::string_view pipeline,std::string_view event);//延迟链路提交失败回包
    //群消息异步接口
    std::shared_ptr<GroupMessagePersistenceService> groupMessagePersistence_;
    SubmitMessageTaskFn submitMessageTask_;//调用线程：负责将任务放入messageThreadPool_队列
    PostToBaseLoopFn postToBaseLoop_;//调用消息工作线程将完成任务放回baseLoop
    std::atomic<bool> acceptingAsyncMessages_{true};//控制当前服务是否还接受新的异步群消息任务
    DispatchResult handleGroupMessageAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);//群消息异步入口
    DispatchResult submitResultMapToDispatchResult(const Request& req,infra::thread::TaskSubmitResult result,std::string_view pipeline="async");//提交结果转换
    
    infra::thread::TaskSubmitResult submitGroupMessagePersistence(PendingGroupMessageContext context);//由baseLoop调用，在限流通过后生成command并提交任务到MYSQL工作线程
    void completeGroupMessageRateLimit(PendingGroupMessageContext context,AsyncRateLimitResult result);//redis工作线程工作线程回投，校验状态和处理结果
    void completeGroupMessage(PendingGroupMessageContext context,GroupMessageWriteCommand command,GroupMessageWriteResult result);//持久化完成并回到baseLoop,根据持久化结果完成群消息业务

    //批量广播接口
    BatchSendFn batchSend_;//由TcpServer注入实际网络发送能力
    std::vector<ConnKey> collectGroupTargets(const std::string& groupId, ConnKey excludedKey) const;//获取群成员在线连接
    net::BatchSendResult sendEncodedPayload(const std::vector<ConnKey>& targets, std::string payload);//接收一份已经完成的JSON编码字符串，构造共享payload,并批量发送给多个连接
    
    //异步私聊消息
    std::shared_ptr<DirectMessagePersistenceService> directMessagePersistence_;//私聊持久化服务
    
    DispatchResult handleDmAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);//私聊消息异步处理
    void completeDirectMessage(PendingDirectMessageContext context,DirectMessageWriteCommand command, DirectMessageWriteResult result);//
    infra::thread::TaskSubmitResult submitDirectMessagePersistence(PendingDirectMessageContext context);//提交任务到数据库线程池
    void completeDirectMessageRateLimit(PendingDirectMessageContext context,AsyncRateLimitResult result);//限流检查完成后处理私聊请求终止或进行持久化


    //异步查询接口
    SubmitDbTaskFn submitDbReadTask_;//数据库读线程池任务提交
    Session* resolvePendingSession(const PendingDbRequestContext& context);//baseLoop调用，检查session

    infra::thread::TaskSubmitResult submitGroupHistoryQuery(PendingGroupHistoryContext context);//提交异步群聊历史任务
    void completeGroupHistoryRateLimit(PendingGroupHistoryContext context,AsyncRateLimitResult result);
    DispatchResult handleGroupHistoryAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);//群聊历史查询异步处理
    void completeGroupHistory(PendingGroupHistoryContext context,AsyncDbResult<std::vector<storage::MessageRecord>> result);

//私聊历史消息异步处理
    infra::thread::TaskSubmitResult submitDmHistoryQuery(PendingDmHistoryContext context);
    void completeDmHistoryRateLimit(PendingDmHistoryContext context,AsyncRateLimitResult result);
    DispatchResult handleDmHistoryAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeDmHistory(PendingDmHistoryContext context,AsyncDbResult<std::vector<storage::DirectMessageRecord>> result);
//离线列表获取
    DispatchResult handleOfflineListAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeOfflineList(PendingOfflineListContext context,AsyncDbResult<std::vector<storage::OfflineMessageIndex>> result);

//异步会话接口
//会话列表查询
    DispatchResult handleConversationListAsync(const Request& request, ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeConversationList(PendingConversationListContext context,AsyncDbResult<std::vector<ConversationView>> result);

//增量同步
    infra::thread::TaskSubmitResult submitSyncQuery(PendingSyncContext context);
    void completeSyncRateLimit(PendingSyncContext context,AsyncRateLimitResult result);
    DispatchResult handleSyncAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeSync(PendingSyncContext context,AsyncDbResult<SyncResult> result);

    //ACK和已读消息异步
    DispatchResult handleMessageAckAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);

    void completeMessageAck(PendingAckContext context,AsyncAckResult result);
    DispatchResult handleOfflineAckAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);


    DispatchResult handleConversationReadAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeConversationRead(PendingConversationReadContext context,AsyncDbResult<storage::ConversationReadResult> result);

    //注册登录异步认证
    SubmitAuthTaskFn submitAuthTask_;
    bool tryBeginAuthOperation(Session& session,AuthOperation operation,std::uint64_t& operationId);//
    Session* resolvePendingAuthSession(const std::weak_ptr<TcpConnection>& connection,ConnKey key,AuthOperation operation,std::uint64_t operationId);//回投到baseLoop后校验session连接与操作是否匹配
    void finishAuthOperation(Session& session,AuthOperation operation,std::uint64_t operationId);//认证操作处理完毕后，检查操作和id猴恢复状态
    //异步注册
    DispatchResult handleRegisterAsync(const Request& request,ConnKey key,Session& session,const std::shared_ptr<TcpConnection>& connection);
    void completeRegisterRateLimit(PendingRegisterContext context,AsyncRateLimitResult result);
    infra::thread::TaskSubmitResult submitRegisterTask(PendingRegisterContext context);
    void completeRegister(PendingRegisterContext context,AsyncAuthResult result);

};


}