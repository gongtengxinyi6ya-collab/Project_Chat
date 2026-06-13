#include "im/ConversationService.h"
#include "storage/UserProfileRepo.h"
#include "storage/GroupRepo.h"
#include <unordered_map>
im::ConversationService::ConversationService(std::shared_ptr<storage::ConversationRepo> conversationRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRep,std::shared_ptr<storage::GroupRepo> groupRepo)
:conversationRepo_(std::move(conversationRepo)),userProfileRepo_(std::move(userProfileRep)),groupRepo_(std::move(groupRepo)){

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
storage::RepoResult im::ConversationService::recordGroupMessage(const std::string&groupId,std::vector<std::string>& memberAccountIds,const std::string&senderAccountId,const std::string& senderUsername,uint64_t msgId,const std::string& content,uint64_t serverTsMs){
    if(groupId.empty()||senderAccountId.empty()||msgId==0){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    std::string preview=content.substr(0,128);
    if(!conversationRepo_){
        return {.status=storage::RepoStatus::Internal,.message="conversationRepo is not avaiable"};
    }
    return conversationRepo_->upsertGroupOnMessage(groupId,memberAccountIds,senderAccountId,senderUsername,msgId,preview,serverTsMs);
}
    


std::vector<im::ConversationService::ConversationView> im::ConversationService::listConversations(const std::string& ownerAccountId,size_t limit){
    if(ownerAccountId.empty()){
        return {};
    }
    if(!conversationRepo_||!userProfileRepo_||!groupRepo_){
        return {};
    }
    const auto& result=conversationRepo_->listConversations(ownerAccountId,limit);
    if(result.empty()){
        return {};
    }
    std::vector<ConversationView> views;//会话列表
    std::vector<std::string> accountIds;//搜集所有type为direct的targetId
    std::vector<std::string> groupIds;//搜集所有type为group的targetId
    for(const auto& summary:result){
        if(summary.type==storage::ConversationType::Direct){
            accountIds.emplace_back(summary.targetId);//加入targetId
        }
        if(summary.type==storage::ConversationType::Group){
            groupIds.emplace_back(summary.targetId);
        }
        ConversationView view;
        view.summary=summary;
        //初步初始化会话列表
        views.emplace_back(std::move(view));
    }
    //批量查询用户资料
    std::unordered_map<std::string,storage::UserProfile> userProfileByAccountId;
    auto userProfiles=userProfileRepo_->findByAccountIds(accountIds);
    for(const auto& userProfile:userProfiles){
        userProfileByAccountId.emplace(userProfile.accountId,userProfile);
    }
    
    //批量查询群资料
    std::unordered_map<std::string,storage::GroupSnapshot> groupSnapshotByGroupId;
    auto groupSnapshots=groupRepo_->findGroupsByIds(groupIds);
    for(const auto& groupSnapshot:groupSnapshots){
        groupSnapshotByGroupId.emplace(groupSnapshot.groupId,groupSnapshot);
    }
    for(auto& view:views){
        if(view.summary.type==storage::ConversationType::Direct){
            auto it=userProfileByAccountId.find(view.summary.targetId);
            if(it!=userProfileByAccountId.end()){
                view.targetUsername=it->second.username;
                view.targetNickname=it->second.nickname;
                view.targetAvatarUrl=it->second.avatarUrl;
            }
        }
        if(view.summary.type==storage::ConversationType::Group){
            auto it=groupSnapshotByGroupId.find(view.summary.targetId);
            if(it!=groupSnapshotByGroupId.end()){
                view.groupName=it->second.groupName;
                view.groupOwnerAccountId=it->second.ownerAccountId;
            }
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