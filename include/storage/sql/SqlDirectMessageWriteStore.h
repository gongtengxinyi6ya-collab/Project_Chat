#pragma once

#include <memory>

#include "storage/DirectMessageWriteStore.h"

namespace storage {

class SqlConnectionPool;
/*负责同一事务内检测接收账号，检查好友关系
插入私聊消息
更新双方会话摘要
更新接收方未读数
写入接收方待投递索引*/

class SqlDirectMessageWriteStore final
    : public DirectMessageWriteStore {
public:
    explicit SqlDirectMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool);

    RepoResult commit(const im::DirectMessageWriteCommand& command) override;//提交消息处理完整事务

private:
    std::shared_ptr<SqlConnectionPool> pool_;
};

}