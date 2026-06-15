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
    RepoResult createGroup(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId) override;
    bool groupExists(const std::string& groupId) override;
    bool isMember(const std::string& groupId,const std::string&accountId)override;
    RepoResult addMember(const std::string& groupId,const std::string& accountId,uint8_t role) override;
    RepoValueResult<uint8_t> getMemberRole(const std::string&groupId,const std::string& accountId)override;//获取成员角色
    RepoResult updateMemberRole(const std::string& groupId,const std::string& accountId,uint8_t role)override;//设置，取消管理员
    RepoResult transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string& newOwner)override;//转移群主身份
    std::vector<GroupMemberRecord> listMemberRecords(const std::string& groupId)override;//获取成员列表
    
    RepoResult removeMember(const std::string& groupId,const std::string& accountId) override;
    
    std::vector<GroupSnapshot> listGroups()override;
    RepoResult createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId);//
    std::vector<GroupSnapshot> findGroupsByIds(const std::vector<std::string>& groupIds)override;//根据多个groupId查询群基础信息，用于会话列表展示

    RepoValueResult<GroupSnapshot> findGroupById(const std::string& groupId)override;//查询群是否存在，是否解散，群主是谁
    RepoValueResult<size_t> countMembers(const std::string& groupId)override;//获取成员数量，用于邀请前检查人数上限
    RepoValueResult<GroupDissolveRecord> dissolveGroup(const std::string& groupId,const std::string& ownerAccountId,int64_t dissolvedAtMs)override;//解散群
private:
    std::unordered_map<std::string,GroupRecord> groups_;//groupId映射GroupRecord
    mutable std::mutex mutex_;//保护groups_的读写
};
}
