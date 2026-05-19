#pragma once
#include <string>
#include <memory>
#include "storage/MessageRepo.h"
#include "SqlConnectionPool.h"

/*用SQL实现Message
负责群消息持久化*/

namespace storage{
class SqlMessageRepo:public MessageRepo{
public:
    explicit SqlMessageRepo(std::shared_ptr<SqlConnectionPool> pool);
    SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string& from,const std::string& content,uint64_t serverTsMs) override;
    std::vector<MessageRecord> listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit)override;//查询群历史消息
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}