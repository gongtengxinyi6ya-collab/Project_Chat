#include "im/ConversationService.h"
#include "storage/UserProfileRepo.h"
#include <unordered_map>
im::ConversationService::ConversationService(std::shared_ptr<storage::ConversationRepo> conversationRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRep)
:conversationRepo_(std::move(conversationRepo)),userProfileRepo_(std::move(userProfileRep)){

}




storage::RepoResult im::ConversationService::recordDirectMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string& senderUsername,uint64_t msgId,const std::string& content,uint64_t serverTsMs){
    if(senderAccountId.empty()||receiverAccountId.empty()||msgId==0){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    std::string preview=content.substr(0,128);
    if(!conversationRepo_){
        return {.status=storage::RepoStatus::Internal,.message="conversationRepo is not avaiable"};
    }
    return conversationRepo_->upsertDirectOnMessage(senderAccountId,receiverAccountId,senderUsername,msgId,preview,serverTsMs);
}



std::vector<im::ConversationService::ConversationView> im::ConversationService::listConversations(const std::string& ownerAccountId,size_t limit){
    if(ownerAccountId.empty()){
        return {};
    }
    if(!conversationRepo_||!userProfileRepo_){
        return {};
    }
    const auto& result=conversationRepo_->listConversations(ownerAccountId,limit);
    if(result.empty()){
        return {};
    }
    std::vector<ConversationView> views;//会话列表
    std::vector<std::string> targetIds;//搜集所有type为direct的targetId
    for(const auto& summary:result){
        if(summary.type==storage::ConversationType::Direct){
            targetIds.emplace_back(summary.targetId);//加入targetId
            ConversationView view;
            view.summary=summary;
            //初步初始化会话列表
            views.emplace_back(std::move(view));
        }
    }
    //查找展示资料
    std::unordered_map<std::string,storage::UserProfile> userProfileByAccountId;
    const auto& userProfiles=userProfileRepo_->findByAccountIds(targetIds);
    for(const auto& userProfile:userProfiles){
        userProfileByAccountId.emplace(userProfile.accountId,userProfile);
    }
    for(auto& view:views){
        auto it=userProfileByAccountId.find(view.summary.targetId);
        if(it!=userProfileByAccountId.end()){
            view.targetUsername=it->second.username;
            view.targetNickname=it->second.nickname;
            view.targetAvatarUrl=it->second.avatarUrl;
        }
    }
    return views;
}
storage::RepoResult im::ConversationService::markRead(const std::string& ownerAccountId,storage::ConversationType type,const std::string& targetId,uint64_t readMsgId,uint64_t readAtMs){
    if(ownerAccountId.empty()||targetId.empty()||readMsgId==0){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!conversationRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    return conversationRepo_->markConversationRead(ownerAccountId,type,targetId,readMsgId,readAtMs);
}