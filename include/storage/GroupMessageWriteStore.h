#pragma once
#include <cstdint>
#include "storage/RepoValueResult.h"
#include "im/GroupMessagePersistenceTypes.h"

/*抽象持久化一条完整群消息的事务处理*/
namespace storage{
class GroupMessageWriteStore{
public:
    virtual ~GroupMessageWriteStore()=default;
    virtual RepoValueResult<std::uint64_t>commit(const im::GroupMessageWriteCommand& command) = 0;

};
}