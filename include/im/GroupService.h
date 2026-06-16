#pragma once
#include <memory>
#include <vector>
#include <string>
#include "storage/RepoResult.h"
#include "storage/RepoValueResult.h"
#include "storage/types/GroupTypes.h"
#include "im/GroupRole.h"
namespace storage{
    class GroupRepo;
    class UserProfileRepo;
    class FriendRepo;

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

struct GroupInviteResult {//群聊邀请结果
    bool joined{false};//新增成员
    bool alreadyIn{false};//目标已经在群
    std::string groupId{};
    std::string targetAccountId{};//被邀请账号
};

struct GroupDissolveResult {//群聊解散结果
    bool dissolved{false};//解散是否完成
    bool alreadyDissolved{false};//之前是否已经解散
    std::string groupId;//目标群
    std::vector<std::string> affectedAccountIds;//解散前所有成员，
};
class GroupService{
public:
    GroupService(GroupManager& GroupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::FriendRepo> friendRepo,bool requireFriendForInvite,size_t maxGroupMembers);
    storage::RepoResult kickMember(const std::string& groupId,const std::string& operatorAccountId,const std::string& targetAccountId);//踢出群聊
    storage::RepoResult setAdmin(const std::string& groupId,const std::string& operatorAccountId,const std::string&targetAccountId,bool enable);//设置管理员
    storage::RepoResult transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string&newOwner);//群主转让
    std::vector<GroupMemberView> listMemberViews(const std::string& groupId);//获取群成员信息
    
    storage::RepoValueResult<GroupInviteResult> inviteMember(const std::string& groupId,const std::string& inviterAccountId,const std::string& targetAccountId);//邀请好友入群
    storage::RepoValueResult<GroupDissolveResult> dissolveGroup(const std::string& groupId,const std::string& operatorAccountId,int64_t nowMs);//解散群聊
    storage::RepoResult reloadGroup(const std::string& groupId);//查数据库进行内存更新
private:
    GroupManager& groupManager_;
    std::shared_ptr<storage::GroupRepo> groupRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
    std::shared_ptr<storage::FriendRepo> friendRepo_;
    bool requireFriendForInvite_{true};
    size_t maxGroupMembers_{500};//群人数上限
    storage::RepoResult reloadGroup(const std::string& groupId);//查数据库进行内存更新
};
}