#include "im/RoomManager.h"

bool im::RoomManager::join(const std::string& room,ConnKey key){
    auto& members=roomMembers_[room];
    auto [it,inserted]=members.insert(key);//结构化绑定
    return inserted;
}
bool im::RoomManager::leave(const std::string& room,ConnKey key){
    auto it=roomMembers_.find(room);
    if(it==roomMembers_.end()){
        return false;
    }
    auto& members=it->second;
    auto member=members.find(key);
    if(member!=members.end()){
        members.erase(member);
        if(members.empty()){
            roomMembers_.erase(room);
        }
        return true;
    }
    return false;
}

std::vector<im::RoomManager::ConnKey> im::RoomManager::members(const std::string& room)const{
    std::vector<ConnKey> result;
    auto it=roomMembers_.find(room);
    if(it!=roomMembers_.end()){
        const auto& members=it->second;
        result.insert(result.end(),members.begin(),members.end());
    }
    return result;
}
size_t im::RoomManager::memberCount(const std::string& room)const{
    auto it=roomMembers_.find(room);
    if(it==roomMembers_.end()){
        return 0;
    }
    auto& members=it->second;
    return members.size();
}
void im::RoomManager::removeKeyEverywhere(ConnKey key,std::optional<std::string> knownRoom){
    if(knownRoom.has_value()){
        leave(knownRoom.value(),key);
        return;
    }
    for(auto it=roomMembers_.begin();it!=roomMembers_.end();){
        auto& members=it->second;
        auto member=members.find(key);
        if(member!=members.end()){
            members.erase(member);
            if(members.empty()){
                it=roomMembers_.erase(it);
            }else{
                ++it;
            }
        }else{
            ++it;
        }
    }
}