#pragma once
#include<memory>
/*统一注入储存依赖*/
namespace storage{

class UserRepo;
class GroupRepo;
class MessageRepo;
class RepositoryBundle{
public:
    std::shared_ptr<UserRepo> userRepo;
    std::shared_ptr<GroupRepo> groupRepo;
    std::shared_ptr<MessageRepo> messageRepo;

    bool valid()const{return userRepo&&groupRepo&&messageRepo;};
};
}