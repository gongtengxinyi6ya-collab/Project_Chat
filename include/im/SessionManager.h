#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>
#include <cstdint>
#include <vector>
#include "Session.h"
#include "net/SendTypes.h"
/*负责session的查/建/删/用户名映射*/
namespace im{
class SessionManager{
public:
    Session& getOrCreate(net::ConnKey);//确保每个连接有Session
    const Session* find(net::ConnKey) const;//查找
    Session* find(net::ConnKey);//查找，非常量
    bool bindUser(net::ConnKey,uint64_t userId,std::string accountId,std::string username);//用户可多连接在线
    void unbindConn(net::ConnKey);//断连解绑在线关系
    std::vector<net::ConnKey> connKeysByAccountId(const std::string &accountId)const;//获取某用户全部在线连接
    std::optional<std::string> accountIdByConn(net::ConnKey)const;//根据连接获取用户
    bool isOnLine(const std::string& accountId)const;//判断用户是否在线
    std::vector<std::string> onLineUsers()const;//返回成员列表
    void erase(net::ConnKey);//删除session
    void clear();//
    size_t removeJoinedGroup(const std::string& accountId,const std::string&groupId);//用户在群聊被踢后从对应session删除groupId
    size_t addJoinedGroup(const std::string& accountId,const std::string& groupId);//邀请入群后同步在线Session
    size_t removeJoinedGroupForAccounts(const std::vector<std::string>& accountIds,const std::string& groupId);//解散时批量清理在线群聊
private:
    std::unordered_map<net::ConnKey,Session> sessions_;//连接到Session的映射
    std::unordered_map<std::string,std::unordered_set<net::ConnKey>> accountConnMap_;//一个用户多连接，支持多端登录
    std::unordered_map<net::ConnKey,std::string> connAccountMap_;//连接到用户账号的映射，方便根据连接获取用户信息
};
}