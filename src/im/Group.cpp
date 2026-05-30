#include "im/Group.h"

im::Group::Group(const std::string& groupid,const std::string& name,const std::string& ownerAccountId)
:groupId_(groupid),name_(name),ownerAccountId_(ownerAccountId)
{   

    createAtMs_=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
}

bool im::Group::addMember(const std::string& accountId){
    if(accountId.empty()){
        return false;
    }
    if(hasMember(accountId)){
        return false;
    }
    memberAccountIds_.insert(accountId);
    return true;
}
bool im::Group::removeMember(const std::string& accountId){
    if(!hasMember(accountId)){
        return false;
    }
    memberAccountIds_.erase(accountId);
    return true;
}
bool im::Group::hasMember(const std::string&accountId)const{
    return memberAccountIds_.contains(accountId);
}
size_t im::Group::memberCount() const{
    return memberAccountIds_.size();
}
std::vector<std::string> im::Group::members() const{
    std::vector<std::string> result;
    result.insert(result.end(),memberAccountIds_.begin(),memberAccountIds_.end());
    return result;
}