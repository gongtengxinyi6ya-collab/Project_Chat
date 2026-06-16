#include "im/GroupJoinService.h"
#include "storage/GroupRepo.h"
#include "storage/GroupJoinRequestRepo.h"
#include "storage/UserProfileRepo.h"
#include "im/GroupManager.h"
#include "im/GroupRole.h"
#include <stdexcept>
im::GroupJoinService::GroupJoinService(GroupManager& GroupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::GroupJoinRequestRepo> joinRequestRepo,size_t maxGroupMembers)
:groupManager_(groupManager_),groupRepo_(std::move(groupRepo)),userProfileRepo_(std::move(userProfileRepo)),joinRequestRepo_(std::move(joinRequestRepo)){
    if(!groupRepo_||!userProfileRepo_||joinRequestRepo_){
        throw std::invalid_argument("GroupService:null dependency");
    }
}
storage::RepoValueResult<storage::GroupJoinApplyResult> im::GroupJoinService::GroupJoinService::applyToJoin(const std::string&groupId,const std::string& appliacntAccountId,const std::string&message,int64_t nowMs){
    if(groupId.empty()||appliacntAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!userProfileRepo_||!joinRequestRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    //检查用户资料存在
    auto resultUser=userProfileRepo_->findByAccountId(appliacntAccountId);
    if(!resultUser){
        return {.status=storage::RepoStatus::UserNotFound};
    }
    //创建申请
    return joinRequestRepo_->submit(groupId,appliacntAccountId,message,nowMs);

}

storage::RepoValueResult<std::vector<storage::GroupJoinRequestRecord>> im::GroupJoinService::GroupJoinService::listPendingRequests(const std::string& groupId,const std::string& operatorAccountId,size_t limit){
    if(groupId.empty()||operatorAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!groupRepo_||!joinRequestRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    //查询操作者角色
    auto resultRole=groupRepo_->getMemberRole(groupId,operatorAccountId);
    if(!resultRole.ok()){
        return {.status=resultRole.status,.message=resultRole.message};
    }
    if(!resultRole.value.has_value()){
        return {.status=storage::RepoStatus::Internal,.message="value invalid"};
    }
    auto role=roleFromUint(resultRole.value.value());
    if(role!=GroupRole::Owner&&role!=GroupRole::Admin){
        return {.status=storage::RepoStatus::NoPermission};
    }
    return joinRequestRepo_->listPending(groupId,limit);
}
storage::RepoValueResult<storage::GroupJoinReviewResult> im::GroupJoinService::GroupJoinService::reviewRequest(const std::string&groupId,const std::string&applicantAccountId,const std::string&reviewerAccountId,bool approve,size_t maxMember,int64_t nowMs){
    if(groupId.empty()||applicantAccountId.empty()||reviewerAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!joinRequestRepo_){
        return {.status=storage::RepoStatus::Internal};
    }
    //审核申请
    auto resultReview=joinRequestRepo_->review(groupId,applicantAccountId,reviewerAccountId,approve,maxMember,nowMs);
    if(!resultReview.ok()){
        return resultReview;
    }
    if(!resultReview.value){
        return {.status=storage::RepoStatus::Internal};
    }
    auto reviewValue=resultReview.value.value();
    //真正通过
    if(reviewValue.approved&&reviewValue.memberAdded){
        //同步内存
        auto result=groupManager_.joinGroup(groupId,applicantAccountId);
        if(result!=JoinResult::OK_JOINED){
            auto resultReload=reloadGroup(groupId);
        if(!resultReload.ok()){
            return {.status=storage::RepoStatus::Internal,.message="database invite the member but memory reload failed"};
        }
        }
    }
    return resultReview;
}
storage::RepoResult im::GroupJoinService::reloadGroup(const std::string& groupId){
    if(groupId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    std::vector<std::string> groupIds;
    groupIds.push_back(groupId);
    auto result=groupRepo_->findGroupsByIds(groupIds);
    if(result.empty()){
        //查询数据库发现为空，则同步删除内存
        groupManager_.removeGroup(groupId);
        return {.status=storage::RepoStatus::NotFound,.message="groupId not found"};
    }
    auto members=groupRepo_->listMemberRecords(groupId);
    auto group=result.front();
    if(!groupManager_.restoreGroup(group.groupId,group.groupName,group.ownerAccountId,members)){
        return {.status = storage::RepoStatus::Internal,.message = "failed to restore group into memory"};
    }
    return {.status=storage::RepoStatus::Ok};

}