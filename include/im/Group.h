#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>
/*群实体*/
namespace im{
class Group{
public:
    Group(const std::string& groupid,const std::string& name,const std::string owner);
    bool addMember(const std::string& user);//加入成员
    bool removeMember(const std::string &user);//移除成员
    bool hasMember(const std::string& user) const;//是否在群
    size_t memberCount()const;//成员数量
    std::vector<std::string> members()const ;//导出成员列表

private:
    std::string groupId_;//群唯一标识
    std::string name_;//群名
    std::string owner_;//群主用户名
    std::unordered_set<std::string> members_;//成员用户名集合
    uint64_t createAtMs_;//创建时间戳
};
}