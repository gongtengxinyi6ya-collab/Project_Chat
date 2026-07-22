#pragma once

#include "im/DirectMessagePersistenceTypes.h"
#include "storage/RepoResult.h"

namespace storage {
/*私聊消息完整事务抽象*/
class DirectMessageWriteStore {
public:
    virtual ~DirectMessageWriteStore() = default;

    virtual RepoResult commit(const im::DirectMessageWriteCommand& command) = 0;
};

}