#pragma once
#include <string>
#include <cstdint>
#include "storage/RepoResult.h"

namespace storage{
class SaveMessageResult{
public:
    RepoStatus status{RepoStatus::Ok};
    uint64_t messageId{0};
    std::string message{};

    bool ok()const{return status==RepoStatus::Ok;}
};

struct MessageRecord{//群聊消息记录
    uint64_t messageId{0};
    std::string groupId{};
    std::string senderAccountId{};
    std::string senderUsername{};
    std::string content{};
    uint64_t serverTsMs{};
};

struct DirectMessageRecord {//私聊消息记录
    uint64_t messageId{0};//全局消息ID，对应msg_id
    std::string conversationKey{};//会话Key,用于快速查询A/B之间的历史
    std::string senderAccountId{};//发送者accountId
    std::string receiverAccountId{};//接收者accountId
    std::string senderUsername{};//发送者用户名
    std::string content{};//消息正文
    uint64_t serverTsMs{0};//服务端时间戳
};

//枚举区分离线索引指向群消息还是私聊消息
enum class OfflineMessageType:uint8_t{
    Unknown=0,
    Group=1,
    Direct=2
};
struct OfflineMessageIndex{
    uint64_t msgId{0};//离线的消息ID
    OfflineMessageType type{OfflineMessageType::Unknown};
    std::string groupId{};//群消息所属群，私聊时为空
    std::string peerAccountId{};//私聊对端账号
};

struct MessageAckResult {//用来表达一次Ack结果
    size_t requestedCount{0};//客户端请求ACK的消息数量
    size_t ackedCount{0};//服务端实际写入或更新成功的数量
    size_t ignoredCount{0};//不属于当前用户，重复ACK，无效msgId等被忽略的数量
};
inline std::string offlineMessageTypeToString(OfflineMessageType type){
    switch(type){
        case OfflineMessageType::Group:
            return "group";
        case OfflineMessageType::Direct:
            return "direct";
        default:
            return "unknown";
    }
}
}