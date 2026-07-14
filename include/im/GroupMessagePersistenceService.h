#pragma once
#include <memory>
#include "GroupMessagePersistenceTypes.h"
namespace storage{
    class MessageRepo;
    class ConversationRepo;
    class OfflineMessageRepo;
}

namespace im{
/*封装群消息的阻塞型持久化操作*/
class GroupMessagePersistenceService{
public:
    GroupMessagePersistenceService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::ConversationRepo> conversationRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo);
    GroupMessageWriteResult persist(const GroupMessageWriteCommand& command) const;//消息持久化
private:
    std::shared_ptr<storage::MessageRepo> messageRepo_;
    std::shared_ptr<storage::ConversationRepo> conversationRepo_;
    std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo_;
};
}