#pragma once
#include <string>
#include <memory>
#include <vector>
#include  <cstdint>
#include "storage/types/GroupTypes.h"
#include "storage/RepoResult.h"
#include "storage/RepoValueResult.h"

namespace storage{
    class GroupRepo;
    class UserProfileRepo;
    class GroupJoinRequestRepo;
}
namespace im{
    class GroupManager;

class GroupJoinService{
public:
    GroupJoinService(GroupManager& GroupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::GroupJoinRequestRepo> joinRequestRepo,size_t maxGroupMembers);
    storage::RepoValueResult<storage::GroupJoinApplyResult> applyToJoin(const std::string&groupId,const std::string& appliacntAccountId,const std::string&message,int64_t nowMs);//申请加群
    storage::RepoValueResult<std::vector<storage::GroupJoinRequestRecord>> listPendingRequests(const std::string& groupId,const std::string& operatorAccountId,size_t limit);//获取申请列表
    storage::RepoValueResult<storage::GroupJoinReviewResult> reviewRequest(const std::string&groupId,const std::string&applicantAccountId,const std::string&reviewerAccountId,bool approve,int64_t nowMs);//审核申请

private:
    GroupManager& groupManager_;
    std::shared_ptr<storage::GroupRepo> groupRepo_;
    std::shared_ptr<storage::UserProfileRepo> userProfileRepo_;
    std::shared_ptr<storage::GroupJoinRequestRepo> joinRequestRepo_;
    size_t maxGroupMembers_;
    storage::RepoResult reloadGroup(const std::string& groupId);//查数据库进行内存更新
};
}