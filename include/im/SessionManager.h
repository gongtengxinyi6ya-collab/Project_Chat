#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>
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
    bool bindUser(ConnKey,std::string);//用户可多连接在线
    void unbindConn(ConnKey);//断连解绑在线关系
    std::vector<ConnKey> connKeysByUser(const std::string &user)const;//获取某用户全部在线连接
    std::optional<std::string> usernameByConn(ConnKey)const;//根据连接获取用户
    bool isOnLine(const std::string& username)const;//判断用户是否在线
    std::vector<std::string> onLineUsers()const;//返回成员列表
    void erase(ConnKey);//删除session
private:
    std::unordered_map<ConnKey,Session> sessions_;
    std::unordered_map<std::string,std::unordered_set<ConnKey>> userConnMap_;//一个用户多连接，支持多端登录
    std::unordered_map<ConnKey,std::string> connUserMap_;
};
}