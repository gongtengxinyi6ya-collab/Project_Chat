#pragma once
#include <unordered_map>
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
    bool bindUser(ConnKey,std::string);//auth成功后绑定，负责唯一性判断
    void unbindUser(ConnKey);//断连清理username映射
    std::optional<ConnKey> connKeyByUser(const std::string &user)const;//DM路由定位
    std::vector<std::string> onLineUsers()const;//返回成员列表
    void erase(ConnKey);//删除session
private:
    std::unordered_map<ConnKey,Session> sessions_;
    std::unordered_map<std::string,ConnKey> userConnMap_;
};
}