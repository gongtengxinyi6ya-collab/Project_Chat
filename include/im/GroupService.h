#pragma once
#include <memory>
#include <vector>
#include <string>
#include "storage/RepoResult.h"
#include "im/GroupRole.h"
namespace storage{
    class GroupRepo;
    class UserProfileRepo;
}
namespace im{
    class GroupManager;

    struct GroupMemberView {
        std::string accountId;
        std::string username;
        std::string nickname;
        std::string avatarUrl;
        GroupRole role{GroupRole::Member};
};
class GroupService{
public:
    GroupService(GroupManager& GroupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo);
    storage::RepoResult kickMember(const std::string& groupId,const std::string& operatorAccountId,const std::string& targetAccountId);//踢出群聊
    storage::RepoResult setAdmin(const std::string& groupId,const std::string& operatorAccountId,const std::string&targetAccountId,bool enable);//设置管理员
    storage::RepoResult transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string&newOwner);//群主转让
    std::vector<GroupMemberView> listMemberViews(const std::string& groupId);//获取群成员信息

    storage::RepoResult reloadGroup(const std::string& groupId);//查数据库进行内存更新
private:
    GroupManager& groupManager_;
    std::shared_ptr<storage::GroupRepo> groupRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
};
}