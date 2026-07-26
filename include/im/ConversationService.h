#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"
#include "im/ConversationTypes.h"
/*会话服务，读写会话列表，并补充目标用户展示资料*/

namespace storage{
    
    class UserProfileRepo;
    class GroupRepo;
    class ConversationRepo;
}
namespace im{
class ConversationService{
public:
    
    ConversationService(std::shared_ptr<storage::ConversationRepo> conversationRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRep,std::shared_ptr<storage::GroupRepo> groupRepo);
    storage::RepoResult recordDirectMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string& senderUsername,uint64_t msgId,const std::string& content,uint64_t serverTsMs);//群消息消息保存成功后，更新群成员会话列表
    storage::RepoResult recordGroupMessage(const std::string&groupId,std::vector<std::string>& memberAccountIds,const std::string&senderAccountId,const std::string& senderUsername,uint64_t msgId,const std::string& content,uint64_t serverTsMs);//私聊消息保存成功后，更新双方会话列表
    storage::RepoValueResult<std::vector<ConversationView>> listConversations(const std::string& ownerAccountId,size_t limit);///给客户端返回可以展示的会话列
private:
    std::shared_ptr<storage::ConversationRepo> conversationRepo_;//读写会话表
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;//获取用户资料
    std::shared_ptr<storage::GroupRepo> groupRepo_;//获取群资料
};
}