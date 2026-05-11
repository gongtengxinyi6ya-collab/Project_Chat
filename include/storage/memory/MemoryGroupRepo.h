#pragma once
#include <unordered_set>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include "storage/GroupRepo.h"

namespace storage{
class MemoryGroupRepo:public storage::GroupRepo{
public:
struct GroupRecord{
    std::string groupId;
    std::string groupName;
    std::string owner;
    std::unordered_set<std::string> members;
};
    storage::RepoResult createGroup(const std::string& groupId,const std::string& groupName,const std::string& owner)override;//创建微信群聊
    bool groupExists(const std::string& groupId)override;//群聊是否存在
    storage::RepoResult addMember(const std::string& groupId,const std::string& username)override;//用户入群时保存群成员关系
    storage::RepoResult removeMember(const std::string& groupId,const std::string& username)override;//主动退群或群主踢人删除关系
    std::vector<std::string> listMembers(const std::string& groupId)override;//服务重建时恢复时可重建GroupManager
private:
    std::unordered_map<std::string,GroupRecord> groups_;//groupId映射GroupRecord
    mutable std::mutex mutex_;//保护groups_的读写
};
}
