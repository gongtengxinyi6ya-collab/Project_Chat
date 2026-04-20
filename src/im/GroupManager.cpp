#include "im/GroupManager.h"
std::pair<bool,std::string> im::GroupManager::createGroup(const std::string& owner,const std::string& name){
    if(owner.empty()||name.empty()){
        return {false,""};
    }
    std::string groupId="Group"+std::to_string(nexxtGroupSeq_++);
    Group g(groupId,name,owner);
    if(!g.addMember(owner)){
        return {false,""};
    }
    groupsById_[groupId]=std::move(g);
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
    if(!isMember(groupId,user)){
        return QuitResult::ERR_NOT_IN_GROUP;
    }
    if(!it->second.removeMember(user)){
        return QuitResult::ERR_NOT_IN_GROUP;
    }
    groupsById_.erase(groupId);
    userGroups_[user].erase(groupId);
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