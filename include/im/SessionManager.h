#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>
#include <cstdint>
#include <vector>
#include "Session.h"
/*负责session的查/建/删/用户名映射*/
namespace im{
class SessionManager{
public:
    using ConnKey=int;
    Session& getOrCreate(ConnKey);//确保每个连接有Session
    const Session* find(ConnKey) const;//查找
    Session* find(ConnKey);//查找，非常量
    bool bindUser(ConnKey,uint64_t userId,std::string accountId,std::string username);//用户可多连接在线
    void unbindConn(ConnKey);//断连解绑在线关系
    std::vector<ConnKey> connKeysByAccountId(const std::string &accountId)const;//获取某用户全部在线连接
    std::optional<std::string> accountIdByConn(ConnKey)const;//根据连接获取用户
    bool isOnLine(const std::string& username)const;//判断用户是否在线
    std::vector<std::string> onLineUsers()const;//返回成员列表
    void erase(ConnKey);//删除session
    void removeJoinedGroup(const std::string& accountId,const std::string&groupId);//用户在群聊被踢后从对应session删除groupId
private:
    std::unordered_map<ConnKey,Session> sessions_;//连接到Session的映射
    std::unordered_map<std::string,std::unordered_set<ConnKey>> accountConnMap_;//一个用户多连接，支持多端登录
    std::unordered_map<ConnKey,std::string> connAccountMap_;//连接到用户账号的映射，方便根据连接获取用户信息
};
}