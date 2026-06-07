#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include "SyncModels.h"

/*负责消息同步聚合：
根据客户端游标拉取增量消息
拉取离线消息索引
组装同步结果*/
namespace storage{
    class MessageRepo;
    class OfflineMessageRepo;
}
namespace im{
class MessageSyncService{
public:
    MessageSyncService(std::shared_ptr<storage::MessageRpo> messageRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo);
    SyncResult sync(const std::string& accountId,const std::vector<SyncCursor>& cursors,size_t offlineLimit);//执行一次账号级同步
    ConversationDelta loadDirectDelta(const std::string& selfAccountId,const std::string& peerAccountId,uint64_t lastMsgId,size_t limit);//拉取某个私聊会话的增量消息
    ConversationDelta loadGroupDelta(const std::string& groupId,uint64_t lastMsgId,size_t limit);//拉取群聊会话增量消息
private:
    std::shared_ptr<storage::MessageRpo> messageRepo_;//查询私聊/群聊历史消息
    std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo_;//查询当前账号离线消息索引
};
}