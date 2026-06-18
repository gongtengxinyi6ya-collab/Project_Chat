#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"
#include "storage/RepoValueResult.h"
#include "storage/types/MessageTypes.h"
#include "storage/types/ConversationTypes.h"
/*后续保存群消息，私聊消息，离线消息*/
namespace storage{
class MessageRepo{
public:
    virtual ~MessageRepo()=default;
    virtual SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string&senderAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs)=0;//保存群消息
    virtual std::vector<MessageRecord> listGroupMessages(const std::string&groupId,uint64_t beforeMsgId,size_t limit)=0;//查询群历史消息
    virtual SaveMessageResult saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs)=0;//保存私聊消息
    virtual std::vector<DirectMessageRecord> listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit)=0;//查询某个私聊会话的历史消息
    virtual std::vector<DirectMessageRecord> listDirectMessagesAfter(const std::string& conversationKey,uint64_t lastMsgId,size_t limit)=0;//查询某个私聊会话客户端本地最后一条消息之后的消息
    virtual std::vector<MessageRecord> listGroupMessagesAfter(const std::string& groupId,uint64_t lastMsgId,size_t limit)=0;//查询某个群聊中msg_id>lastMsgId的新消息
    virtual RepoValueResult<size_t> markDelivered(const std::string&accountId,const std::vector<uint64_t>& msgIds,int64_t deliveredAtMs)=0;//把一批消息标记为当前账号已送达
    virtual RepoValueResult<size_t> markReadBefore(const std::string&accountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,int64_t readAtMs)=0;//把某个会话中msg_id<=readMsgId的消息标记为当前用户已读

};
}