#pragma once
#include "storage/GroupMessageWriteStore.h"
#include <memory>

namespace storage{
    class SqlConnectionPool pool_;

class SqlGroupMessageWriteStore:public GroupMessageWriteStore{
public:
    explicit SqlGroupMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool);
    RepoValueResult<std::uint64_t> commit(const im::GroupMessageWriteCommand& command)override;
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}