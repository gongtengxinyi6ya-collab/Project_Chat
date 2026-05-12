#pragma once
#include <memory>
#include <string>
#include "storage/GroupRepo.h"
#include "SqlConnectionPool.h"

/*用SQL实现GroupRepo
负责群基础信息和群成员关系持久化*/

namespace storage{
class SqlGroupRepo:public GroupRepo{
public:
    explicit SqlGroupRepo(std::shared_ptr<SqlConnectionPool> pool);
    RepoResult createGroup(const std::string& groupId,const std::string& groupName,const std::string& owner) override;
    bool groupExists(const std::string& groupId) override;
    RepoResult addMember(const std::string& groupId,const std::string& username) override;
    RepoResult removeMember(const std::string& groupId,const std::string& username) override;
    std::vector<std::string> listMembers(const std::string& groupId) override;

private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}