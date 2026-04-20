#include "im/Group.h"
im::Group::Group(const std::string& groupid,const std::string& name,const std::string owner)
:groupId_(groupid),name_(name),owner_(owner)
{
    createAtMs_=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
}
bool im::Group::addMember(const std::string& user){
    if(hasMember(user)){
        return false;
    }
    members_.insert(user);
    return true;
}
bool im::Group::removeMember(const std::string& user){
    if(!hasMember(user)){
        return false;
    }
    members_.erase(user);
    return true;
}
bool im::Group::hasMember(const std::string&user)const{
    return members_.find(user)!=members_.end();
}
size_t im::Group::memberCount() const{
    return members_.size();
}
std::vector<std::string> im::Group::members() const{
    std::vector<std::string> result;
    result.insert(result.end(),members_.begin(),members_.end());
    return result;
}