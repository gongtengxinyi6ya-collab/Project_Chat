#pragma once
#include <memory>
#include <string>
#include <optional>
#include "storage/UserProfileRepo.h"
/*FriendService:连接存储层与IM协议层
查询好友列表
判断两人是否为好友关系
承接好友申请审批*/

namespace storage{
    class FriendRepo;
    
}
namespace im
{
class FriendService{
public:
    FriendService(std::shared_ptr<storage::FriendRepo> friendRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo);
    std::optional<storage::UserProfile> findUser(const std::string& accountId)const;//按唯一账号搜索用户
    std::vector<storage::UserProfile> listFriends(const std::string& account)const;//查询好友资料列表
    bool areFriends(const std::string&accountId,const std::string& targetAccountId)const;//判断私聊双方是否有好友关系


private:
    std::shared_ptr<storage::FriendRepo> friendRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
};
}
