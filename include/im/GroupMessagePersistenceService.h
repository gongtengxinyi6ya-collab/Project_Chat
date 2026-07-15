#pragma once
#include <memory>
#include "GroupMessagePersistenceTypes.h"
namespace storage{
    class GroupMessageWriteStore;
}

namespace im{
/*封装群消息的阻塞型持久化操作*/
class GroupMessagePersistenceService{
public:
    GroupMessagePersistenceService(std::shared_ptr<storage::GroupMessageWriteStore> writeStore);
    GroupMessageWriteResult persist(const GroupMessageWriteCommand& command) const;//消息持久化
private:
    std::shared_ptr<storage::GroupMessageWriteStore> writeStore_;
};
}