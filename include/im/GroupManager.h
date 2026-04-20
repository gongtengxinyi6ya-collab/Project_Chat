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
class GroupManager{
public:
    std::pair<bool,std::string> createGroup(const std::string& owner,const std::string& name);//创建群并让owner自动入群
    JoinResult joinGroup(const std::string &groupId,const std::string& user);//用户入群
    QuitResult leaveGroup(const std::string& groupId,const std::string& user);//用户退群
    std::vector<std::string> members(const std::string & groupId) const;//获取成员用户名列表
    std::vector<std::string> groupsOfUser(const std::string& user)const;//取用户加入的所有群
    bool isMember(const std::string& groupId,const std::string& user) const;//群存在且成员包含user
    bool exists(const std::string& groupId)const;//group是否存在
private:
    std::unordered_map<std::string,std::unordered_set<std::string>>  userGroups_;//user映射groupId
    std::unordered_map<std::string,Group> groupsById_;//groupId映射Group主储存
    std::atomic<uint64_t> nexxtGroupSeq_{1};//生成groupId的递增序列,用于生成唯一groupId
};
}
