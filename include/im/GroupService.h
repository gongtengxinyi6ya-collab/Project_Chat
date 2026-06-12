#pragma once
#include <memory>
namespace storage{
    class GroupRepo;
    class UserProfileRepo;
}
namespace im{
    class GroupManager;
class GroupService{

private:
    GroupManager& groupManager_;
    std::shared_ptr<storage::GroupRepo> groupRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
};
}