#pragma once
#include "storage/ConversationReadWriteStore.h"
#include <memory>

namespace storage{
class SqlConnectionPool;
class SqlConnection;

class SqlConversationReadWriteStore final: public ConversationReadWriteStore{
public:
    explicit SqlConversationReadWriteStore(std::shared_ptr<SqlConnectionPool> pool);//接收消息连接池
    RepoValueResult<ConversationReadResult> commit(const ConversationReadCommand& command) override;//获取连接，开启事务，按会话类型分发并提交

private:
    std::shared_ptr<SqlConnectionPool> pool_;//获取消息专用数据库连接

    RepoValueResult<ConversationReadResult> commitDirect(SqlConnection& connection,const ConversationReadCommand& command);//处理私聊游标，未读数和回执
    RepoValueResult<ConversationReadResult> commitGroup(SqlConnection& connection,const ConversationReadCommand& command);//处理群成员已读游标
};
}