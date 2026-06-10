#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include "im/Group.h"

//群关系管理器
namespace im{
enum class JoinResult{
    OK_JOINED,
    OK_ALREADY_IN,
    ERR_NO_SUCH_GROUP
};
enum class QuitResult{
    OK_LEFT,
    ERR_NOT_IN_GROUP,
    ERR_NO_SUCH_GROUP
};
struct GroupMemberInfo{//成员信息，带成员角色
    std::string accountId;
    GroupRole role{GroupRole::Member};
};
class GroupManager{

public:
    std::pair<bool,std::string> createGroup(const std::string& ownerAccountId,const std::string& groupname);//创建群并让owner自动入群
    JoinResult joinGroup(const std::string &groupId,const std::string& accountId);//用户入群
    QuitResult leaveGroup(const std::string& groupId,const std::string& accountId);//用户退群
    bool removeGroup(const std::string& groupId);//删除群,仅当群不存在成员时成功
    std::vector<std::string> members(const std::string & groupId) const;//获取成员用户名列表
    std::vector<std::string> groupsOfUser(const std::string& accountId)const;//取用户加入的所有群
    bool isMember(const std::string& groupId,const std::string& accountId) const;//群存在且成员包含user
    bool exists(const std::string& groupId)const;//group是否存在
    bool restoreGroup(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId,const std::vector<std::string>& members);//启动时恢复群对象和成员集合
    std::optional<GroupRole> roleOf(const std::string& groupId,const std::string&accountId)const;//
    bool isOwner(const std::string&groupId,const std::string& accountId)const;//判断是否为群主
    bool canManageMember(const std::string& groupId, const std::string& operatorAccountId, const std::string& targetAccountId) const;//管理权限判断
    bool setMemberRole(const std::string& groupId, const std::string& accountId, GroupRole role);//设置群角色
    std::vector<GroupMemberInfo> memberInfos(const std::string& groupId) const;//获取群成员
    bool transferOwner(const std::string& groupId, const std::string& oldOwner, const std::string& newOwner);//群主转让
private:
    std::unordered_map<std::string,std::unordered_set<std::string>>  accountIdGroups_;//accountId映射groupId
    std::unordered_map<std::string,Group> groupsById_;//groupId映射Group主储存
    std::atomic<uint64_t> nextGroupSeq_{1};//生成groupId的递增序列,用于生成唯一groupId
};
}
