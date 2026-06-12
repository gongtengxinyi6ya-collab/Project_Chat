#pragma once
#include <memory>
#include <string>
#include "storage/GroupRepo.h"


/*用SQL实现GroupRepo
负责群基础信息和群成员关系持久化*/

namespace storage{
    class SqlConnectionPool;
class SqlGroupRepo:public GroupRepo{
public:
    explicit SqlGroupRepo(std::shared_ptr<SqlConnectionPool> pool);
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
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}