#pragma once
#include "storage/GroupMessageWriteStore.h"
#include <memory>

/*实现抽象类：
负责使用连接和事务，完整提交一条群消息*/
namespace storage{
    class SqlConnectionPool;

class SqlGroupMessageWriteStore:public GroupMessageWriteStore{
public:
    explicit SqlGroupMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool);
    RepoValueResult<std::uint64_t> commit(const im::GroupMessageWriteCommand& command)override;
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}