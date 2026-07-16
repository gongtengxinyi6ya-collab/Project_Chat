#pragma once
#include <string>
#include <memory>
#include "storage/MessageRepo.h"


/*用SQL实现Message
负责群消息持久化*/

namespace storage{
    class SqlConnectionPool;
class SqlMessageRepo:public MessageRepo{
public:
    explicit SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool);
    SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string& senderAccountId,const std::string& senderUsername,const std::string& content,uint64_t serverTsMs) override;
    std::vector<MessageRecord> listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit)override;//查询群历史消息
    SaveMessageResult saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs) override;//保存私聊消息
    std::vector<DirectMessageRecord> listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit) override;//查询某个私聊会话的历史消息
    std::vector<DirectMessageRecord> listDirectMessagesAfter(const std::string& conversationKey,uint64_t lastMsgId,size_t limit)override;//查询某个私聊会话客户端本地最后一条消息之后的消息
    std::vector<MessageRecord> listGroupMessagesAfter(const std::string& groupId,uint64_t lastMsgId,size_t limit)override;//查询某个群聊中msg_id>lastMsgId的新消息
    
    RepoValueResult<MessageAckResult> markDeliveredBatch(const std::string&accountId,const std::vector<uint64_t>& msgIds,int64_t deliveredAtMs)override;//把一批消息标记为当前账号已送达
    RepoValueResult<size_t> markReadBefore(const std::string&accountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,int64_t readAtMs)override;//把某个会话中msg_id<=readMsgId的消息标记为当前用户已读
    

private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}