#pragma once
#include "storage/ConversationRepo.h"
#include <memory>
/*实现ConversationRepo抽象*/

namespace storage{
class SqlConnectionPool;
class SqlConversationRepo:public ConversationRepo{
public:
    explicit SqlConversationRepo(std::shared_ptr<SqlConnectionPool>pool);
    RepoResult upsertDirectOnMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs)override;//私聊消息发送成功落库后，更新发送方和接收方的会话行
    std::vector<ConversationSummary> listConversations(const std::string& ownerAccountId,size_t limit)override;//查询某个账号会话列表
    RepoResult markConversationRead(const std::string&ownerAccountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,uint64_t readAtMs)override;//清空某个会话未读数，并记录读到哪个消息
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}