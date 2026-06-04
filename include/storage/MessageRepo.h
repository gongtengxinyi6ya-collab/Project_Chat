#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"

/*后续保存群消息，私聊消息，离线消息*/
namespace storage{
class SaveMessageResult{
public:
    RepoStatus status{RepoStatus::Ok};
    uint64_t messageId{0};
    std::string message{};

    bool ok()const{return status==RepoStatus::Ok;}
};
class MessageRepo{
public:
struct MessageRecord{
    uint64_t messageId{0};
    std::string groupId{};
    std::string senderAccountId{};
    std::string senderUsername{};
    std::string content{};
    uint64_t serverTsMs{};
};
//私聊消息结构
struct DirectMessageRecord {
    uint64_t messageId{0};//全局消息ID，对应msg_id
    std::string conversationKey{};//会话Key,用于快速查询A/B之间的历史
    std::string senderAccountId{};//发送者accountId
    std::string receiverAccountId{};//接收者accountId
    std::string senderUsername{};//发送者用户名
    std::string content{};//消息正文
    uint64_t serverTsMs{0};//服务端时间戳
};
    virtual ~MessageRepo()=default;
    virtual SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string&senderAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs)=0;//保存群消息
    virtual std::vector<MessageRecord> listGroupMessages(const std::string&groupId,uint64_t beforeMsgId,size_t limit)=0;//查询群历史消息
    virtual SaveMessageResult saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs)=0;//保存私聊消息
    virtual std::vector<DirectMessageRecord> listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit)=0;//查询某个私聊会话的历史消息
    virtual std::vector<DirectMessageRecord> listDirectMessagesAfter(const std::string& conversationKey,uint64_t lastMsgId,size_t limit)=0;//查询某个私聊会话客户端本地最后一条消息之后的消息
    
};
}