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
    auto [it, success] = groupsById_.emplace(groupId,std::move(g));
    if (!success) {
        return {false,""};
    }
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
bool im::GroupManager::restoreGroup(const std::string& groupId,const std::string& groupName,const std::string&ownerAccountId,const std::vector<storage::GroupMemberRecord>& members){
    if(groupId.empty()||groupName.empty()||ownerAccountId.empty()){
        return false;
    }
    if(!exists(groupId)){
        Group g(groupId,groupName,ownerAccountId);
        for(auto& member:members){
            if(!g.addMember(member.accountId,static_cast<GroupRole>(member.role))){
                continue;
            }
            accountIdGroups_[member.accountId].insert(groupId);//恢复成员成功时同步添加映射
        }
        groupsById_.emplace(groupId,std::move(g));//同步保存群
    }
    //解析Group数字，更新nextGroupSeq_以避免groupId冲突，例如Group1,Group2等
    if(groupId.rfind("Group",0)==0){//如果groupId以"Group"开头
        try{
            uint64_t idNum=std::stoull(groupId.substr(5));
            uint64_t expectedNextSeq=idNum+1;
            uint64_t currentNextSeq=nextGroupSeq_.load();
            while(currentNextSeq<expectedNextSeq){
                nextGroupSeq_.compare_exchange_weak(currentNextSeq,expectedNextSeq);
            }
        }
        catch(const std::exception& e){
            //解析失败时不更新nextGroupSeq_
        }
    }
    return true;
}

std::optional<im::GroupRole> im::GroupManager::roleOf(const std::string& groupId,const std::string&accountId)const{
    auto it=groupsById_.find(groupId);
    if(it!=groupsById_.end()){
        return it->second.roleOf(accountId);
    }
    return std::nullopt;
}
bool im::GroupManager::isOwner(const std::string&groupId,const std::string& accountId)const{
    auto role=roleOf(groupId,accountId);
    if(role){
        if(role.value()==GroupRole::Owner){
            return true;
        }
    }
    return false;
}
bool im::GroupManager::canManageMember(const std::string& groupId, const std::string& operatorAccountId, const std::string& targetAccountId) const{
     auto roleOfOperator=roleOf(groupId,operatorAccountId);//获取操作者角色
     auto roleOfTarget=roleOf(groupId,targetAccountId);
    if(roleOfOperator&&roleOfTarget){
        if(roleOfOperator.value()==GroupRole::Owner){//群主可以管理其他成员
            return true;
        }
        else if(roleOfOperator.value()==GroupRole::Admin&&roleOfTarget.value()==GroupRole::Member){
            //管理员只能管理普通成员
            return true;
        }
        return false;
    }
    return false;
}
bool im::GroupManager::setMemberRole(const std::string& groupId, const std::string& accountId, GroupRole role){
    if(isMember(groupId,accountId)){
        auto it=groupsById_.find(groupId);
        if(it->second.setRole(accountId,role)){
            return true;
        }
    }
    return false;

}
std::vector<im::GroupMemberInfo> im::GroupManager::memberInfos(const std::string& groupId) const{
    auto it=groupsById_.find(groupId);
    if(it!=groupsById_.end()){
        std::vector<GroupMemberInfo> info;
        auto members=it->second.members();
        for(const auto& member:members){
            info.emplace_back(GroupMemberInfo{.accountId=member.first,.role=member.second});
        }
        return info;
    }
    return {};
}
bool im::GroupManager::transferOwner(const std::string& groupId, const std::string& oldOwner, const std::string& newOwner){
    auto it=groupsById_.find(groupId);
    if(it!=groupsById_.end()){
        if(it->second.isOwner(oldOwner)){
            return it->second.transFerOwner(oldOwner,newOwner);
        }
    }
    return false;
}