#include "im/SessionManager.h"

im::Session& im::SessionManager::getOrCreate(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        return it->second;
    }
    Session session;
    sessions_[key]=session;
    return sessions_[key];
}
const im::Session* im::SessionManager::find(ConnKey key)const{
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        return &it->second;
    }
    return nullptr;
}
im::Session* im::SessionManager::find(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        return &it->second;
    }
    return nullptr;
}
bool im::SessionManager::bindUser(ConnKey key,uint64_t userId,std::string accountId,std::string name){
    sessions_[key].userId_=userId;
    sessions_[key].accountId_=accountId;
    sessions_[key].username_=name;
    sessions_[key].state_=im::ConnState::Authed;
    accountConnMap_[accountId].insert(key);
    connAccountMap_[key]=accountId;
    return true;
}
void im::SessionManager::unbindConn(ConnKey key){
    auto it=connAccountMap_.find(key);
    if(it==connAccountMap_.end()){
        return;
    }
    accountConnMap_[it->second].erase(key);
    if(accountConnMap_[it->second].empty()){
        accountConnMap_.erase(it->second);
    }
    connAccountMap_.erase(key);
}
std::vector<im::SessionManager::ConnKey> im::SessionManager::connKeysByAccountId(const std::string& accountId) const{
    auto it=accountConnMap_.find(accountId);
    if(it!=accountConnMap_.end()){
        std::vector<ConnKey> keys;
        keys.insert(keys.end(),it->second.begin(),it->second.end());
        return keys;
    }
    return {};
}
std::optional<std::string> im::SessionManager::accountIdByConn(ConnKey key)const{
    auto it=connAccountMap_.find(key);
    if(it==connAccountMap_.end()){
        return std::nullopt;
    }
    return it->second;
}
bool im::SessionManager::isOnLine(const std::string& accountId)const{
    auto it=accountConnMap_.find(accountId);
    if(it==accountConnMap_.end()){
        return false;
    }
    return !it->second.empty();
}
std::vector<std::string> im::SessionManager::onLineUsers()const{
    std::vector<std::string> accountIds;
    for(const auto& it:accountConnMap_){
        accountIds.push_back(it.first);
    }
    return accountIds;
}
void im::SessionManager::erase(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        sessions_.erase(key);
    }
}

size_t im::SessionManager::removeJoinedGroup(const std::string& accountId,const std::string& groupId){
    //根据账号获取所有在线连接
    auto keys=connKeysByAccountId(accountId);
    size_t count=0;
    for(const auto& key:keys){
        //遍历所有key找到对应session
        auto it=sessions_.find(key);
        if(it!=sessions_.end()){
            it->second.joinedGroupIds_.erase(groupId);
            count++;
        }
    }
    return count;
}

size_t im::SessionManager::addJoinedGroup(const std::string& accountId,const std::string& groupId){
    //根据账号获取所有在线连接
    auto keys=connKeysByAccountId(accountId);
    size_t count=0;
    for(const auto& key:keys){
        //遍历所有key找到对应session
        auto it=sessions_.find(key);
        if(it!=sessions_.end()){
            it->second.joinedGroupIds_.insert(groupId);
            count++;
        }
    }
    return count;
}
size_t im::SessionManager::removeJoinedGroupForAccounts(const std::vector<std::string>& accountIds,const std::string& groupId){
    size_t count=0;
    for(const auto& accountId:accountIds){
        count+=removeJoinedGroup(accountId,groupId);
    }
    return count;
}