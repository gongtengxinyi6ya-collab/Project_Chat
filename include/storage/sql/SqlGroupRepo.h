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
    RepoResult addMember(const std::string& groupId,const std::string& accountId) override;
    RepoResult removeMember(const std::string& groupId,const std::string& accountId) override;
    std::vector<std::string> listMembers(const std::string& groupId) override;
    std::vector<GroupSnapshot> listGroups()override;
    RepoResult createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId);//
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}