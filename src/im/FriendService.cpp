#include "im/FriendService.h"
#include "storage/FriendRepo.h"
#include <stdexcept>
im::FriendService::FriendService(std::shared_ptr<storage::FriendRepo> friendRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo){
    if(!friendRepo||!userProfileRepo){
        throw std::invalid_argument("Repo is empty");
    }
    friendRepo_=std::move(friendRepo);
    userProfileRepo_=std::move(userProfileRepo);
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
