#pragma once
#include<memory>
/*统一注入储存依赖*/
namespace storage{

class UserRepo;
class GroupRepo;
class MessageRepo;
class OfflineMessageRepo;
class UserSessionRepo;
class RepositoryBundle{
public:
    std::shared_ptr<UserRepo> userRepo;
    std::shared_ptr<GroupRepo> groupRepo;
    std::shared_ptr<MessageRepo> messageRepo;
    std::shared_ptr<OfflineMessageRepo> offlineMessageRepo;//注入离线消息存储
    std::shared_ptr<UserSessionRepo> userSessionRepo;//
    bool valid()const{return userRepo&&groupRepo&&messageRepo;};
};
}