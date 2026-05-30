#pragma once
#include <string>
#include <vector>

#include <unordered_set>
#include <chrono>
/*群实体*/
namespace im{
class Group{

public:
    Group(const std::string& groupid,const std::string& name,const std::string& ownerAccountId);
    bool addMember(const std::string& accountId);//加入成员
    bool removeMember(const std::string &accountId);//移除成员
    bool hasMember(const std::string& accountId) const;//是否在群
    size_t memberCount()const;//成员数量
    std::vector<std::string> members()const ;//导出成员列表

private:
    std::string groupId_;//群唯一标识
    std::string name_;//群名
    std::string ownerAccountId_;//群主用户账号
    std::unordered_set<std::string> memberAccountIds_;//成员用户accountId映射成员信息；
    uint64_t createAtMs_;//创建时间戳
};
}