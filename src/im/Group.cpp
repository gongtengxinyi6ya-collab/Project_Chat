#include "im/Group.h"

im::Group::Group(const std::string& groupid,const std::string& name,const std::string& ownerAccountId)
:groupId_(groupid),name_(name),ownerAccountId_(ownerAccountId)
{   

    createAtMs_=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
}

bool im::Group::addMember(const std::string& accountId,GroupRole role){
    if(accountId.empty()){
        return false;
    }
    if(hasMember(accountId)){
        return false;
    }
    members_.emplace(accountId,role);
    return true;
}
bool im::Group::removeMember(const std::string& accountId){
    if(!hasMember(accountId)){
        return false;
    }
    if(accountId==ownerAccountId_){
        return false;
    }
    members_.erase(accountId);
    return true;
}
bool im::Group::hasMember(const std::string&accountId)const{
    return members_.contains(accountId);
}
size_t im::Group::memberCount() const{
    return members_.size();
}
std::optional<im::GroupRole> im::Group::roleOf(const std::string&accountId)const{
    auto it=members_.find(accountId);
    if(it!=members_.end()){
        return it->second;
    }
    return std::nullopt;
}
bool im::Group::isOwner(const std::string&accountId)const{
    accountId==ownerAccountId_;
}
bool im::Group::isAdminOrOwner(const std::string& accountId)const{
    auto it=members_.find(accountId);
    if(it!=members_.end()){
        return it->second==GroupRole::Admin;
    }
    return false;
}

bool im::Group::setRole(const std::string&accountId,GroupRole role){
    if(accountId==ownerAccountId_){
        return false;
    }
    auto it=members_.find(accountId);
    if(it!=members_.end()){
        it->second=role;
        return true;
    }
    return false;
}