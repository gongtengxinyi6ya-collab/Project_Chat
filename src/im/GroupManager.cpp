#include "im/GroupManager.h"
std::pair<bool,std::string> im::GroupManager::createGroup(const std::string& owner,const std::string& name){
    if(owner.empty()||name.empty()){
        return {false,""};
    }
    std::string groupId="Group"+std::to_string(nextGroupSeq_++);
    Group g(groupId,name,owner);
    if(!g.addMember(owner)){
        return {false,""};
    }
    groupsById_.emplace(groupId,std::move(g));
    userGroups_[owner].insert(groupId);
    return {true, groupId};
}
im::JoinResult im::GroupManager::joinGroup(const std::string& groupId,const std::string& user){
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end()){
        return JoinResult::ERR_NO_SUCH_GROUP;
    }
    if(isMember(groupId,user)){
        return JoinResult::OK_ALREADY_IN;
    }
    if(!it->second.addMember(user)){
        return JoinResult::OK_ALREADY_IN;
    }
    userGroups_[user].insert(groupId);
    return JoinResult::OK_JOINED;
}
im::QuitResult im::GroupManager::leaveGroup(const std::string& groupId,const std::string& user){
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end()){
        return QuitResult::ERR_NO_SUCH_GROUP;
    }
    if(!it->second.removeMember(user)){
        return QuitResult::ERR_NOT_IN_GROUP;
    }
    auto userIt=userGroups_.find(user);
    if(userIt!=userGroups_.end()){
        userGroups_[user].erase(groupId);
        if(userGroups_[user].empty()){
            userGroups_.erase(user);
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
std::vector<std::string> im::GroupManager::groupsOfUser(const std::string& user) const{
    auto it=userGroups_.find(user);
    if(it!=userGroups_.end()){
        std::vector<std::string> groups;
        groups.insert(groups.end(),it->second.begin(),it->second.end());
        return groups;
    }
    return {};
}
bool im::GroupManager::isMember(const std::string& groupId,const std::string& user)const{
    auto it=groupsById_.find(groupId);
    if(it==groupsById_.end())
        return false;
    return it->second.hasMember(user);
}
bool im::GroupManager::exists(const std::string& groupId)const{
    return groupsById_.count(groupId);
}
bool im::GroupManager::restoreGroup(const std::string& groupId,const std::string& groupName,const std::string&owner,const std::vector<std::string>& members){
    if(groupId.empty()||groupName.empty()||owner.empty()){
        return false;
    }
    if(!exists(groupId)){
        Group g(groupId,groupName,owner);
        for(auto& member:members){
            if(!g.addMember(member)){
                continue;
            }
            userGroups_[member].insert(groupId);//恢复成员成功时同步添加映射
        }
        groupsById_.emplace(groupId,std::move(g));//同步保存群
    }
    return true;
}