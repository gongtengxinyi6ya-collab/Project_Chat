#pragma once
#include <memory>
#include <string>
#include <optional>
#include "storage/UserProfileRepo.h"
#include "storage/RepoValueResult.h"
#include "storage/FriendRequestRepo.h"
/*FriendService:连接存储层与IM协议层
查询好友列表
判断两人是否为好友关系
承接好友申请审批*/

namespace storage{
    class FriendRepo;
    class FriendRequestRepo;
    
}
namespace im
{
struct FriendRequestView{
    uint64_t requestId{0};//申请ID
    std::string requesterAccountId{};//发起人账号
    std::string username{};//发起人用户名
    std::string nickname{};//发起人昵称
    std::string avatarUrl{};//发起人头像
    int64_t createdAtMs{0};//申请时间
};
class FriendService{
public:
    FriendService(std::shared_ptr<storage::FriendRepo> friendRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::FriendRequestRepo> friendRequestRepo);
    std::optional<storage::UserProfile> findUser(const std::string& accountId)const;//按唯一账号搜索用户
    std::vector<storage::UserProfile> listFriends(const std::string& account)const;//查询好友资料列表
    bool areFriends(const std::string&accountId,const std::string& targetAccountId)const;//判断私聊双方是否有好友关系
    storage::RepoResult removeFriend(const std::string& accountId,const std::string&targetAccountId);//删除好友
    storage::RepoValueResult<uint64_t> sendRequest(const std::string& from,const std::string& to,int64_t nowMs);//发起好友申请
    std::vector<FriendRequestView> listIncomingRequests(const std::string& accountId);//查询待处理申请，并批量查询发起人资料
    storage::RepoValueResult<storage::FriendRequest> acceptRequest(const std::string& accountId,uint64_t requestId,int64_t nowMs);//委托Repo执行事务化同意
    storage::RepoValueResult<storage::FriendRequest> rejectRequest(const std::string&accountId,uint64_t requestId,int64_t nowMs);//委托Repo拒绝申请

private:
    std::shared_ptr<storage::FriendRepo> friendRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
    std::shared_ptr<storage::FriendRequestRepo> friendRequestRepo_;//管理好友申请持久化
};
}
