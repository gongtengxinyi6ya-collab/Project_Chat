#include "storage/memory/MemoryGroupRepo.h"

storage::RepoResult storage::MemoryGroupRepo::createGroup(const std::string& groupId,const std::string& groupName,const std::string& owner){
    RepoResult result;
    if(groupId.empty()||groupName.empty()||owner.empty()){
        result.status=RepoStatus::InvalidArgument;
        return result;
    }
    GroupRecord group;
    group.groupId=groupId;
    group.groupName=groupName;
    group.owner=owner;
    group.members.insert(owner);
    {   
        std::lock_guard lk(mutex_);
        auto it=groups_.find(groupId);
        if(it==groups_.end()){
        result.status=RepoStatus::AlreadyExists;
        return result;
    }
        groups_.emplace(groupId,std::move(group));//emplace插入大对象更快捷
    }
    return result;
}
storage::RepoResult storage::MemoryGroupRepo::addMember(const std::string&groupId,const std::string& username,[[maybe_unused]]uint8_t role){
    RepoResult result;
    std::lock_guard lk(mutex_);
    auto it=groups_.find(groupId);
    if(it==groups_.end()){
        result.status=RepoStatus::NotFound;
        return result;
    }
    
    if(it->second.members.count(username)){
        result.status=RepoStatus::AlreadyExists;
        return result;
    }
    it->second.members.insert(username);
    return result;
}
storage::RepoResult storage::MemoryGroupRepo::removeMember(const std::string& groupId,const std::string& username){
    RepoResult result;
    std::lock_guard lk(mutex_);
    if(!groupExists(groupId)){
        result.status=RepoStatus::NotFound;
        return result;
    }
    auto it=groups_.find(groupId);
    if(it->second.members.count(username)){
        it->second.members.erase(username);
        return result;
    }
    result.status=RepoStatus::NotFound;
    return result;
}

bool storage::MemoryGroupRepo::groupExists(const std::string&groupId){
    auto it=groups_.find(groupId);
    return it!=groups_.end();
}
bool storage::MemoryGroupRepo::isMember(const std::string& groupId,const std::string& accountId){
    auto it=groups_.find(groupId);
    if(it==groups_.end()){
        return false;
    }
    return it->second.members.count(accountId);
}
std::vector<storage::GroupSnapshot> storage::MemoryGroupRepo::listGroups(){
    std::lock_guard lk(mutex_);
    if(!groups_.empty()){
        std::vector<GroupSnapshot> groupsBasicMess;
        for(auto& it:groups_){
            GroupSnapshot groupSnapshot;
            groupSnapshot.groupId=it.second.groupId;
            groupSnapshot.groupName=it.second.groupName;
            groupSnapshot.ownerAccountId=it.second.owner;
            groupsBasicMess.emplace_back(std::move(groupSnapshot));
        }
        return groupsBasicMess;
    }
    return {};
}
std::vector<storage::GroupSnapshot> storage::MemoryGroupRepo::findGroupsByIds([[maybe_unused]]const std::vector<std::string>& groupIds){
    return {GroupSnapshot{}};
}

//空实现其他方法
storage::RepoValueResult<uint8_t> storage::MemoryGroupRepo::getMemberRole([[maybe_unused]]const std::string&groupId,[[maybe_unused]]const std::string& accountId){
    return RepoValueResult<uint8_t>{.status=RepoStatus::Ok,.value=0};
}
storage::RepoResult storage::MemoryGroupRepo::updateMemberRole([[maybe_unused]]const std::string& groupId,[[maybe_unused]]const std::string& accountId,[[maybe_unused]]uint8_t role){
    return RepoResult{.status=RepoStatus::Ok};
}
storage::RepoResult storage::MemoryGroupRepo::transferOwner([[maybe_unused]]const std::string& groupId,[[maybe_unused]]const std::string& oldOwner,[[maybe_unused]]const std::string& newOwner){
    return RepoResult{.status=RepoStatus::Ok};
}
std::vector<storage::GroupMemberRecord> storage::MemoryGroupRepo::listMemberRecords([[maybe_unused]]const std::string& groupId){
    return {};
}
storage::RepoResult storage::MemoryGroupRepo::createGroupWithOwner([[maybe_unused]]const std::string& groupId,[[maybe_unused]]const std::string& groupName,[[maybe_unused]]const std::string& ownerAccountId){
    return createGroup(groupId,groupName,ownerAccountId);
}