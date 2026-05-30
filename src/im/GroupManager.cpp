#include "im/GroupManager.h"
std::pair<bool,std::string> im::GroupManager::createGroup(const std::string& ownerAccountId,const std::string& name){
    if(ownerAccountId.empty()||name.empty()){
        return {false,""};
    }
    std::string groupId="Group"+std::to_string(nextGroupSeq_++);
    Group g(groupId,name,ownerAccountId);
    if(!g.addMember(ownerAccountId)){
        return {false,""};
    }
    groupsById_.emplace(groupId,std::move(g));
    accountIdGroups_[ownerAccountId].insert(groupId);
    return {true, groupId};
}
im::JoinResult im::GroupManager::joinGroup(const std::string& groupId,const std::string& accountId){
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end()){
        return JoinResult::ERR_NO_SUCH_GROUP;
    }
    if(isMember(groupId,accountId)){
        return JoinResult::OK_ALREADY_IN;
    }
    if(!it->second.addMember(accountId)){
        return JoinResult::OK_ALREADY_IN;
    }
    accountIdGroups_[accountId].insert(groupId);
    return JoinResult::OK_JOINED;
}
im::QuitResult im::GroupManager::leaveGroup(const std::string& groupId,const std::string& accountId){
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end()){
        return QuitResult::ERR_NO_SUCH_GROUP;
    }
    if(!it->second.removeMember(accountId)){
        return QuitResult::ERR_NOT_IN_GROUP;
    }
    auto userIt=accountIdGroups_.find(accountId);
    if(userIt!=accountIdGroups_.end()){
        accountIdGroups_[accountId].erase(groupId);
        if(accountIdGroups_[accountId].empty()){
            accountIdGroups_.erase(accountId);
        }
    }
    
    return QuitResult::OK_LEFT;
}
bool im::GroupManager::removeGroup(const std::string& groupId){
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end()){
        return false;
    }
    if(it->second.memberCount()>0){
        return false;
    }
    groupsById_.erase(it);
    return true;
}
std::vector<std::string> im::GroupManager::members(const std::string & groupId) const{
    auto it=groupsById_.find(groupId);
    if(it!=groupsById_.end()){
        return it->second.members();
    }
    return {};
}
std::vector<std::string> im::GroupManager::groupsOfUser(const std::string& accountId) const{
    auto it=accountIdGroups_.find(accountId);
    if(it!=accountIdGroups_.end()){
        std::vector<std::string> groups;
        groups.insert(groups.end(),it->second.begin(),it->second.end());
        return groups;
    }
    return {};
}
bool im::GroupManager::isMember(const std::string& groupId,const std::string& accountId)const{
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end())
        return false;
    return it->second.hasMember(accountId);
}
bool im::GroupManager::exists(const std::string& groupId)const{
    return groupsById_.count(groupId);
}
bool im::GroupManager::restoreGroup(const std::string& groupId,const std::string& groupName,const std::string&ownerAccountId,const std::vector<std::string>& members){
    if(groupId.empty()||groupName.empty()||ownerAccountId.empty()){
        return false;
    }
    if(!exists(groupId)){
        Group g(groupId,groupName,ownerAccountId);
        for(auto& member:members){
            if(!g.addMember(ownerAccountId)){
                continue;
            }
            accountIdGroups_[ownerAccountId].insert(groupId);//恢复成员成功时同步添加映射
        }
        groupsById_.emplace(groupId,std::move(g));//同步保存群
    }
    return true;
}