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
    RepoResult createGroup(const std::string& groupId,const std::string& groupName,const std::string& owner) override;
    bool groupExists(const std::string& groupId) override;
    RepoResult addMember(const std::string& groupId,const std::string& accountId,const std::string& username) override;
    RepoResult removeMember(const std::string& groupId,const std::string& accountId) override;
    std::vector<GroupMember> listMembers(const std::string& groupId) override;
    std::vector<GroupSnapshot> listGroups()override;
    RepoResult createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& owner);//
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}