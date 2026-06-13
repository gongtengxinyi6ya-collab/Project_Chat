#include "storage/GroupRepo.h"
storage::RepoResult storage::GroupRepo::createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& owner){
    auto result=createGroup(groupId,groupName,owner);
    if(!result.ok()&&result.status!=RepoStatus::AlreadyExists){
        return result;
    }
    auto result2=addMember(groupId,owner,2);
    if(!result2.ok()&&result2.status!=RepoStatus::AlreadyExists){
        return result2;
    }
    return RepoResult{.status=RepoStatus::Ok};
}