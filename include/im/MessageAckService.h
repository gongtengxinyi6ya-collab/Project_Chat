#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "storage/RepoResult.h"
#include "storage/ConversationRepo.h"

/*消息确认服务：
确认某些消息被客户端收到
确认离线消息索引被客户端处理
标记某个会话已读
保证ACK，未读数，离线消息清理之间语义一致*/
namespace storage{
    class MessageRepo;
    class OfflineMessageRepo;
}
namespace im{
class MessageAckService{
public:
    MessageAckService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo,std::shared_ptr<storage::ConversationRepo> conversationRepo);//储存接口注入
    storage::RepoResult ackMessages(const std::string& accountId,const std::vector<uint64_t>& msgIds,int64_t ackAtMs);//送达ACK，表示客户端已经收到消息
    storage::RepoResult ackOfflineMessages(const std::string&accountId,const std::vector<uint64_t>& offlineMsgIds);//客户端确认已经拿到离线消息索引
    storage::RepoResult markConversationRead(const std::string&accountId,storage::ConversationType type,const std::string&targetId,uint64_t readMsgId,int64_t readAtMs);//标记会话已读
    
private:
    std::shared_ptr<storage::MessageRepo> messageRepo_;//标记消息已送达，已读
    std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo_;//操作离线消息索引
    std::shared_ptr<storage::ConversationRepo> conversationRepo_;//标记会话已读，清理未读数
};
}