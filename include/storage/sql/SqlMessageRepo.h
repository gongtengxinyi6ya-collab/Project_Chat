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
    std::vector<MessageRecord> listGroupMessageAfter(const std::string& groupId,uint64_t lastMsgId,size_t limit)override;//查询某个群聊中msg_id>lastMsgId的新消息
    

private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}