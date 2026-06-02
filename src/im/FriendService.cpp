#include "im/FriendService.h"
#include "storage/FriendRepo.h"
#include <stdexcept>
#include <unordered_map>
#include "storage/FriendRequestRepo.h"
im::FriendService::FriendService(std::shared_ptr<storage::FriendRepo> friendRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::FriendRequestRepo> friendRequestRepo){
    if(!friendRepo||!userProfileRepo||!friendRequestRepo){
        throw std::invalid_argument("Repo is empty");
    }
    friendRepo_=std::move(friendRepo);
    userProfileRepo_=std::move(userProfileRepo);
    friendRequestRepo_=std::move(friendRequestRepo);
}
std::optional<storage::UserProfile> im::FriendService::findUser(const std::string& accountId)const{
    if(accountId.empty()){
        return std::nullopt;
    }
    if(!userProfileRepo_){
        return std::nullopt;
    }
    auto profile=userProfileRepo_->findByAccountId(accountId);
    if(profile){
        return profile.value();
    }
    return std::nullopt;
}

std::vector<storage::UserProfile> im::FriendService::listFriends(const std::string& accountId)const{
    if(accountId.empty()){
        return {};
    }
    //获取好友列表
    if(!friendRepo_||!userProfileRepo_){
        return {};
    }
    auto friendAccountIds=friendRepo_->listFriendAccountIds(accountId);
    //获取profile列表
    return userProfileRepo_->findByAccountIds(friendAccountIds);
}

bool im::FriendService::areFriends(const std::string& accountId,const std::string& targetAccountId)const{
    if(accountId.empty()||targetAccountId.empty()){
        return false;
    }
    if(!friendRepo_){
        return false;
    }
    return friendRepo_->areFriends(accountId,targetAccountId);
}
storage::RepoValueResult<uint64_t> im::FriendService::sendRequest(const std::string&from,const std::string& to,int64_t nowMs){
    //校验参数
    if(from.empty()||to.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(from==to){
        return {.status=storage::RepoStatus::CannotAddYourself,.message="Can not add yourself"};
    }
    //搜索目标用户是否存在
    auto searchUser=findUser(to);
    if(!searchUser){
        return {.status=storage::RepoStatus::NotFound};
    }
    //校验双方尚未成为好友
    if(areFriends(from,to)){
        return {.status=storage::RepoStatus::AlreadyFriends,.message="The user is already your friend"};
    }
    if(!friendRequestRepo_){
        return {.status=storage::RepoStatus::Internal,.message="friendRequestRepo is empty"};
    }
    return friendRequestRepo_->createPendingRequest(from,to,nowMs);
}
std::vector<im::FriendRequestView> im::FriendService::listIncomingRequests(const std::string& accountId){
    if(accountId.empty()){
        return {};
    }
    if(!friendRequestRepo_){
        return {};
    }
    //查询待处理申请
    auto pendingRequestsResult=friendRequestRepo_->listPendingIncoming(accountId);
    if(!pendingRequestsResult.ok()||!pendingRequestsResult.value||pendingRequestsResult.value.value().empty()){
        return {};
    }
    const auto& friendRequests=pendingRequestsResult.value.value();
    std::vector<std::string> accountIds;//待处理申请发起人账号
    std::vector<FriendRequestView> views;
    for(const auto& friendRequest:friendRequests){
        FriendRequestView view;
        view.requestId=friendRequest.requestId;
        view.requesterAccountId=friendRequest.requestAccountId;
        view.createdAtMs=friendRequest.createdAtMs;
        accountIds.push_back(friendRequest.requestAccountId);
        views.emplace_back(std::move(view));
    }
    //批量查询发起人资料
    if(!userProfileRepo_){
        return {};
    }
    const auto& result=userProfileRepo_->findByAccountIds(accountIds);
    if(result.empty()){
        return {};
    }
    std::unordered_map<std::string,storage::UserProfile> profileByaccountId;
    for(const auto& profile:result){
        profileByaccountId.emplace(profile.accountId,profile);
    }
    for(auto& view:views){
        auto it=profileByaccountId.find(view.requesterAccountId);
        if(it!=profileByaccountId.end()){
            view.username=it->second.username;
            view.nickname=it->second.nickname;
            view.avatarUrl=it->second.avatarUrl;
        }
    }
    return views;
}
storage::RepoResult im::FriendService::acceptRequest(const std::string& accountId,uint64_t requestId,int64_t nowMs){
    if(accountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!friendRequestRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    return friendRequestRepo_->acceptPendingAndCreateFriendPair(requestId,accountId,nowMs);
}
storage::RepoResult im::FriendService::rejectRequest(const std::string& accountId,uint64_t requestId,int64_t nowMs){
    if(accountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!friendRequestRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    return friendRequestRepo_->rejectPending(requestId,accountId,nowMs);
}