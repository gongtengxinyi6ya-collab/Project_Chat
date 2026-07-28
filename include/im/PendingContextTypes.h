#pragma
#include <string>
#include <cstdint>
#include <memory>

#include "net/SendTypes.h"
#include "im/ImMessage.h"
#include "im/HistoryQuery.h"
#include "im/SyncModels.h"
class TcpConnection;
namespace im{
using ConnKey=net::ConnKey;

struct PendingGroupMessageContext {//异步上下文
    std::weak_ptr<TcpConnection> senderConnection;//发送者连接
    ConnKey senderKey{0};//连接标识
    Request request;

    std::string senderAccountId;
    std::string senderUsername;
    std::string groupId;
    std::string content;
};

struct PendingDirectMessageContext {//异步私聊上下文
    std::weak_ptr<TcpConnection> senderConnection;
    ConnKey senderKey{0};
    Request request;

    std::string senderAccountId;
    std::string senderUsername;
    std::string receiverAccountId;

    std::string conversationKey;
    std::string content;
};

struct PendingDbRequestContext {//通用异步数据库查询上下文
    std::weak_ptr<TcpConnection> connection;
    ConnKey key{0};
    Request request;

    std::string accountId;
};

struct PendingDmHistoryContext {//私聊历史上下文
    PendingDbRequestContext base;

    std::string peerAccountId;
    std::string conversationKey;
    HistoryQuery query;
};

struct PendingOfflineListContext {
    PendingDbRequestContext base;
    std::size_t limit{20};
};
struct PendingConversationListContext {//会话列表上下文
    PendingDbRequestContext base;
    std::size_t limit{20};
};

struct PendingSyncContext {//增量同步上下文
    PendingDbRequestContext base;
    std::vector<SyncCursor> cursors;
    std::size_t offlineLimit{100};
};

struct AsyncAckResult {
    storage::RepoResult result{
        .status = storage::RepoStatus::Ok
    };

    storage::MessageAckResult messageAck{};
    std::size_t offlineAcked{0};

    std::int64_t queueWaitUs{0};
    std::int64_t executeUs{0};
    std::string exceptionMessage{};

    bool ok() const noexcept {
        return result.ok();
    }
};

struct PendingAckContext {
    PendingDbRequestContext base;

    std::vector<std::uint64_t> messageIds;
    std::vector<std::uint64_t> offlineMessageIds;

    MsgType responseType{
        MsgType::MESSAGE_ACK_RESP
    };

    std::int64_t ackAtMs{0};
};

struct PendingConversationReadContext {
    PendingDbRequestContext base;

    storage::ConversationType conversationType{
        storage::ConversationType::Unknown
    };

    std::string targetId;
    std::uint64_t readMsgId{0};
    std::int64_t readAtMs{0};
};

struct AsyncAuthResult {
    auth::AuthResult value{};
    std::int64_t queueWaitUs{0};
    std::int64_t executeUs{0};
    std::string exceptionMessage{};
};

struct AsyncLogoutResult {
    auth::LogoutResult value{};
    std::int64_t queueWaitUs{0};
    std::int64_t executeUs{0};
    std::string exceptionMessage{};
};
struct PendingRegisterContext {//注册上下文
    std::weak_ptr<TcpConnection> connection;
    ConnKey key{0};
    Request responseRequest;
    std::uint64_t operationId{0};
    std::string peerIp;
    std::string username;
    std::string password;
};
struct PendingLoginContext {//登录上下文
    std::weak_ptr<TcpConnection> connection;
    ConnKey key{0};
    Request responseRequest;
    std::uint64_t operationId{0};
    std::string accountId;
    std::string password;
};
struct PendingLoginContext {//token登录上下文
    std::weak_ptr<TcpConnection> connection;
    ConnKey key{0};
    Request responseRequest;
    std::uint64_t operationId{0};
    std::string accountId;
    std::string tokrn;
};
}