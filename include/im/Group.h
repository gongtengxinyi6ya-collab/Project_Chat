#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <chrono>
#include "im/GroupRole.h"
/*群实体*/
namespace im{
class Group{

public:
    Group(const std::string& groupid,const std::string& name,const std::string& ownerAccountId);
    bool addMember(const std::string& accountId,GroupRole role=GroupRole::Member);//加入成员
    bool removeMember(const std::string &accountId);//移除成员
    bool hasMember(const std::string& accountId) const;//是否在群
    size_t memberCount()const;//成员数量
    const std::unordered_map<std::string,GroupRole>& members()const{return members_;}//导出成员列表
    const std::string& groupId()const{return groupId_;}//返回群id
    const std::string& name()const{return name_;}
    const std::string& ownerAccountId()const{return ownerAccountId_;}//
    std::optional<GroupRole> roleOf(const std::string&accountId)const;//返回对应角色
    bool isOwner(const std::string&accountId)const;
    bool isAdminOrOwner(const std::string& accountId)const;
    bool setRole(const std::string&accountId,GroupRole role);
    bool transFerOwner(const std::string& oldOwner,const std::string& newOwner);

private:
    std::string groupId_;//群唯一标识
    std::string name_;//群名
    std::string ownerAccountId_;//群主用户账号
    std::unordered_map<std::string,GroupRole> members_;//成员用户accountId映射成员信息；
    uint64_t createAtMs_;//创建时间戳
};
}