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
bool im::SessionManager::bindUser(ConnKey key,std::string name){
    if(userConnMap_.count(name)){
        return false;
    }
    sessions_[key].username_=name;
    sessions_[key].state_=im::ConnState::Authed;
    userConnMap_[name].insert(key);
    connUserMap_[key]=name;
    return true;
}
void im::SessionManager::unbindConn(ConnKey key){
    auto it=connUserMap_.find(key);
    if(it==connUserMap_.end()){
        return;
    }
    userConnMap_[it->second].erase(key);
    if(userConnMap_[it->second].empty()){
        userConnMap_.erase(it->second);
    }
    connUserMap_.erase(key);
}
std::vector<im::SessionManager::ConnKey> im::SessionManager::connKeysByUser(const std::string& user) const{
    auto it=userConnMap_.find(user);
    if(it!=userConnMap_.end()){
        std::vector<ConnKey> keys;
        keys.insert(keys.end(),it->second.begin(),it->second.end());
    }
    return {};
}
std::optional<std::string> im::SessionManager::usernameByConn(ConnKey key)const{
    auto it=connUserMap_.find(key);
    if(it==connUserMap_.end()){
        return std::nullopt;
    }
    return it->second;
}
bool im::SessionManager::isOnLine(const std::string& username)const{
    auto it=userConnMap_.find(username);
    if(it==userConnMap_.end()&&it->second.empty()){
        return false;
    }
    return true;
}
std::vector<std::string> im::SessionManager::onLineUsers()const{
    std::vector<std::string> usernames;
    for(const auto& it:userConnMap_){
        usernames.push_back(it.first);
    }
    return usernames;
}
void im::SessionManager::erase(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        sessions_.erase(key);
    }
}