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
storage::RepoResult storage::MemoryGroupRepo::addMember(const std::string&groupId,const std::string& username){
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
std::vector<std::string> storage::MemoryGroupRepo::listMembers(const std::string& groupId){
    std::lock_guard lk(mutex_);
    if(!groupExists(groupId)){
        return {};
    }
    std::vector<std::string> members;
    auto it=groups_.find(groupId);
    members.insert(members.end(),it->second.members.begin(),it->second.members.end());
    return members;
}
bool storage::MemoryGroupRepo::groupExists(const std::string&groupId){
    auto it=groups_.find(groupId);
    return it!=groups_.end();
}
std::vector<storage::GroupRepo::GroupSnapshot> storage::MemoryGroupRepo::listGroups(){
    std::lock_guard lk(mutex_);
    if(!groups_.empty()){
        std::vector<GroupSnapshot> groupsBasicMess;
        for(auto& it:groups_){
            GroupSnapshot groupSnapshot;
            groupSnapshot.groupId=it.second.groupId;
            groupSnapshot.groupName=it.second.groupName;
            groupSnapshot.owner=it.second.owner;
            groupsBasicMess.emplace_back(std::move(groupSnapshot));
        }
        return groupsBasicMess;
    }
    return {};
}