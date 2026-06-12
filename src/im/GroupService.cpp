#include "im/GroupService.h"
#include "storage/GroupRepo.h"
#include "storage/UserProfileRepo.h"
#include "im/GroupManager.h"
#include <stdexcept>
im::GroupService::GroupService(GroupManager& groupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo)
:groupManager_(groupManager),groupRepo_(std::move(groupRepo)),userProfileRepo_(std::move(userProfileRepo)){
    if(!groupRepo_||!userProfileRepo_){
        throw std::invalid_argument("GroupService:null dependency");
    }
}

storage::RepoResult im::GroupService::kickMember(const std::string& groupId,const std::string& operatorAccountId,const std::string& targetAccountId){
    if(groupId.empty()||operatorAccountId.empty()||targetAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    if(!groupRepo_->groupExists(groupId)){
        return {.status=storage::RepoStatus::NotFound,.message="Group is not exist"};
    }
    if(!groupRepo_->isMember(groupId,targetAccountId)||!groupRepo_->isMember(groupId,operatorAccountId)){
        return {.status=storage::RepoStatus::NotFound,.message="Member is not in the group"};
    }
    if(!groupManager_.canManageMember(groupId,operatorAccountId,targetAccountId)){
        return {.status=storage::RepoStatus::Forbidden,.message="not authority to kick the member"};
    }
    auto result=groupRepo_->removeMember(groupId,targetAccountId);
    if(!result.ok()){
        return result;
    }
    groupManager_.leaveGroup(groupId,targetAccountId);
    return {.status=storage::RepoStatus::Ok};
}
storage::RepoResult im::GroupService::setAdmin(const std::string& groupId,const std::string& operatorAccountId,const std::string&targetAccountId,bool enable){

}
storage::RepoResult im::GroupService::transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string&newOwner){

}
std::vector<im::GroupMemberView> im::GroupService::listMemberViews(const std::string& groupId){

}