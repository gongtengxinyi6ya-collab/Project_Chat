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
    userConnMap_[name]=key;
    return true;
}
void im::SessionManager::unbindUser(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        std::string name=it->second.username_;
        if(!name.empty()){
            auto pair=userConnMap_.find(name);
            if(pair!=userConnMap_.end()&&pair->second==key){
                userConnMap_.erase(name);
            }
        }
    }
}
std::optional<im::SessionManager::ConnKey> im::SessionManager::connKeyByUser(const std::string& user) const{
    if(user.empty()){
        return std::nullopt;
    }
    auto it=userConnMap_.find(user);
    if(it==userConnMap_.end()){
        return std::nullopt;
    }
    return it->second;
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