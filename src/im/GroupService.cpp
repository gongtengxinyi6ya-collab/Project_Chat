#include "im/GroupService.h"
#include "storage/GroupRepo.h"
#include "storage/UserProfileRepo.h"
#include "storage/FriendRepo.h"
#include "im/GroupManager.h"
#include <stdexcept>
#include "logger/LogMacros.h"
#include <unordered_map>
#include "storage/types/GroupTypes.h"
im::GroupService::GroupService(GroupManager& groupManager,std::shared_ptr<storage::GroupRepo> groupRepo,std::shared_ptr<storage::UserProfileRepo> userProfileRepo,std::shared_ptr<storage::FriendRepo> friendRepo,bool requireFriendForInvite,size_t maxGroupMembers)
:groupManager_(groupManager),groupRepo_(std::move(groupRepo)),userProfileRepo_(std::move(userProfileRepo)),friendRepo_(std::move(friendRepo)),requireFriendForInvite_(requireFriendForInvite),maxGroupMembers_(maxGroupMembers){
    if(!groupRepo_||!userProfileRepo_){
        throw std::invalid_argument("GroupService:null dependency");
    }
}

storage::RepoResult im::GroupService::kickMember(const std::string& groupId,const std::string& operatorAccountId,const std::string& targetAccountId){
    if(groupId.empty()||operatorAccountId.empty()||targetAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    //确认群存在
    if(!groupRepo_->groupExists(groupId)){
        return {.status=storage::RepoStatus::NotFound,.message="Group is not exist"};
    }
    //确认群成员在群内
    if(!groupRepo_->isMember(groupId,targetAccountId)||!groupRepo_->isMember(groupId,operatorAccountId)){
        return {.status=storage::RepoStatus::TargetNotInGroup,.message="Member is not in the group"};
    }
    //判断权限
    auto roleOfoperator=groupRepo_->getMemberRole(groupId,operatorAccountId);
    if(!roleOfoperator.ok()||!roleOfoperator.value.has_value()){
        return {.status=roleOfoperator.status,.message=roleOfoperator.message};
    }
    auto roleOfTarget=groupRepo_->getMemberRole(groupId,targetAccountId);
    if(!roleOfTarget.ok()||!roleOfTarget.value.has_value()){
        return {.status=roleOfTarget.status,.message=roleOfTarget.message};
    }
    
    bool allowed=roleFromUint(roleOfoperator.value.value())==GroupRole::Owner||(roleFromUint(roleOfoperator.value.value())==GroupRole::Admin&&roleFromUint(roleOfTarget.value.value())==GroupRole::Member);
    if(!allowed){
        return {.status=storage::RepoStatus::NoPermission,.message="not authority to kick the member"};
    }
    auto result=groupRepo_->removeMember(groupId,targetAccountId);
    if(!result.ok()){
        return result;
    }
    //同步更新内存
    if(!groupManager_.removeMember(groupId,targetAccountId)){//内存踢出成员失败
        LOG_ERROR("GroupManager remove member failed, reload group: " + groupId);
        //更新内存
        auto reloadResult=reloadGroup(groupId);
        if(!reloadResult.ok()){
            return {.status=storage::RepoStatus::Internal,.message="database updated but memory reload failed"};
        }
    }
    return {.status=storage::RepoStatus::Ok};
}
storage::RepoResult im::GroupService::setAdmin(const std::string& groupId,const std::string& operatorAccountId,const std::string&targetAccountId,bool enable){
    if(groupId.empty()||operatorAccountId.empty()||targetAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    //群主不能设置自己为管理员
    if(operatorAccountId==targetAccountId){
        return {.status=storage::RepoStatus::InvalidGroupRole,.message="Can not set the owner to admin"};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    auto roleOfOperator=groupRepo_->getMemberRole(groupId,operatorAccountId);
    if(!roleOfOperator.ok()||!roleOfOperator.value.has_value()){
        return {.status=roleOfOperator.status,.message=roleOfOperator.message};
    }
    //确定群主在操作
    if(roleFromUint(roleOfOperator.value.value())!=GroupRole::Owner){
        return {.status=storage::RepoStatus::NoPermission,.message="Only the owener can set admin"};
    }

    //目标为群成员
    if(!groupRepo_->isMember(groupId,targetAccountId)){
        return {.status=storage::RepoStatus::TargetNotInGroup,.message="TargetaccountId is not in the group"};
    }
    if(enable){
        auto result=groupRepo_->updateMemberRole(groupId,targetAccountId,1);
        if(!result.ok()){
            return  {.status=result.status,.message=result.message};
        }
        if(!groupManager_.setMemberRole(groupId,targetAccountId,GroupRole::Admin)){
            LOG_ERROR("GroupManager ser member role failed, reload group: " + groupId);
            //更新内存
            auto reloadResult=reloadGroup(groupId);
            if(!reloadResult.ok()){
                return {.status=storage::RepoStatus::Internal,.message="database updated but memory reload failed"};
            }
        }
    }
    else{
        auto result=groupRepo_->updateMemberRole(groupId,targetAccountId,0);
        if(!result.ok()){
            return  result;
        }
        if(!groupManager_.setMemberRole(groupId,targetAccountId,GroupRole::Member)){
            LOG_ERROR("GroupManager ser member role failed, reload group: " + groupId);
            //更新内存
            auto reloadResult=reloadGroup(groupId);
            if(!reloadResult.ok()){
                return {.status=storage::RepoStatus::Internal,.message="database updated but memory reload failed"};
            }
    }
    }
   
    return {.status=storage::RepoStatus::Ok};

}
storage::RepoResult im::GroupService::transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string&newOwner){
    if(groupId.empty()||oldOwner.empty()||newOwner.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    //群主不能转让给自己
    if(oldOwner==newOwner){
        return {.status=storage::RepoStatus::InvalidArgument,.message="Can not transfer to self"};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    auto roleOfOperator=groupRepo_->getMemberRole(groupId,oldOwner);
    if(!roleOfOperator.ok()||!roleOfOperator.value){
        return {.status=roleOfOperator.status,.message=roleOfOperator.message};
    }
    //确定群主在操作
    if(static_cast<GroupRole>(roleOfOperator.value.value())!=GroupRole::Owner){
        return {.status=storage::RepoStatus::NoPermission,.message="Only the owener can transfer"};
    }
    //目标为群成员
    if(!groupRepo_->isMember(groupId,newOwner)){
        return {.status=storage::RepoStatus::TargetNotInGroup,.message="TargetaccountId is not in the group"};
    }
    auto result=groupRepo_->transferOwner(groupId,oldOwner,newOwner);
    if(!result.ok()){
        return result;
    }
    if(!groupManager_.transferOwner(groupId,oldOwner,newOwner)){
        LOG_ERROR("GroupManager transfer owner failed, reload group: " + groupId);
        //更新内存
        auto reloadResult=reloadGroup(groupId);
        if(!reloadResult.ok()){
            return {.status=storage::RepoStatus::Internal,.message="database updated but memory reload failed"};
        }
    }
    return {.status=storage::RepoStatus::Ok};
}
std::vector<im::GroupMemberView> im::GroupService::listMemberViews(const std::string& groupId){
    if(groupId.empty()){
        return {};
    }
    if(!userProfileRepo_){
        return {};
    }
    //从groupManager获取用户
    auto members=groupManager_.memberInfos(groupId);
    std::vector<GroupMemberView> views;
    std::vector<std::string> memberAccountIds;//获取成员账号
    for(const auto& member:members){
        memberAccountIds.push_back(member.accountId);
        views.emplace_back(GroupMemberView{.accountId=member.accountId,.role=member.role});
    }
    //批量查用户资料
    std::unordered_map<std::string,storage::UserProfile> userProfileByaccountId;//用户账号映射用户资料
    auto result=userProfileRepo_->findByAccountIds(memberAccountIds);
    for(const auto& profile:result){
        userProfileByaccountId.emplace(profile.accountId,profile);
    }
    //合并资料
    for(auto&view:views){
        auto it=userProfileByaccountId.find(view.accountId);
        if(it!=userProfileByaccountId.end()){
            view.username=it->second.username;
            view.avatarUrl=it->second.avatarUrl;
            view.nickname=it->second.nickname;
        }
    }
    return views;

}

storage::RepoResult im::GroupService::reloadGroup(const std::string& groupId){
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

storage::RepoValueResult<im::GroupInviteResult> im::GroupService::inviteMember(const std::string& groupId,const std::string& inviterAccountId,const std::string& targetAccountId){
    if(groupId.empty()||inviterAccountId.empty()||targetAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(inviterAccountId==targetAccountId){
        return {.status=storage::RepoStatus::CannotInivteSelf,.message="can not invite yourself"};
    }
    if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo is not avaiable"};
    }
    //查看群状态
    auto resultGroup=groupRepo_->findGroupById(groupId);
    if(!resultGroup.ok()){
        return {.status=resultGroup.status,.message=resultGroup.message};
    }
    if(!resultGroup.value.has_value()){
        return {.status=storage::RepoStatus::NotFound};
    }
    if(resultGroup.value.value().status==storage::GroupStatus::Dissolved){
        return {.status=storage::RepoStatus::GroupDissolved,.message="The group has been dissolved"};
    }
    //检查邀请者是否在群内
    if(!groupRepo_->isMember(groupId,inviterAccountId)){
        return {.status=storage::RepoStatus::TargetNotInGroup};
    }
    //检查目标账号存在
    if(!userProfileRepo_->findByAccountId(targetAccountId).has_value()){
        return {.status=storage::RepoStatus::NotFound};
    }
    //若限制只能好友邀请
    if(requireFriendForInvite_){
        if(!friendRepo_){
            return {.status=storage::RepoStatus::Internal,.message="friendRepo is not avaiable"};
        }
        if(!friendRepo_->areFriends(inviterAccountId,targetAccountId)){
            return {.status=storage::RepoStatus::InviteRequestsFriend,.message="only can invite your friend into the group"};
        }
    }
    //检查目标是否已经入群
    if(groupRepo_->isMember(groupId,targetAccountId)){//幂等返回
        return {.status=storage::RepoStatus::Ok,.value=GroupInviteResult{.joined=false,.alreadyIn=true,.groupId=groupId,.targetAccountId=targetAccountId}};
    }
    //检查成员上限
    auto resultCount=groupRepo_->countMembers(groupId);
    if(!resultCount.ok()){
        return {.status=resultCount.status,.message=resultCount.message};
    }
    if(!resultCount.value.has_value()){
        return {.status=storage::RepoStatus::Internal};
    }
    if(resultCount.value.value()>maxGroupMembers_){
        return {.status=storage::RepoStatus::GroupMemberLimitReach};
    }
    //加入数据库
    auto resultAdd=groupRepo_->addMember(groupId,targetAccountId,static_cast<uint8_t>(GroupRole::Member));
    if(!resultAdd.ok()){
        return {.status=resultAdd.status,.message=resultAdd.message};
    }
    //同步内存
    auto resultManager=groupManager_.joinGroup(groupId,targetAccountId);
    if(resultManager!=JoinResult::OK_JOINED){
        auto resultReload=reloadGroup(groupId);
        if(!resultReload.ok()){
            return {.status=storage::RepoStatus::Internal,.message="database invite the member but memory reload failed"};
        }
    }
    return {.status=storage::RepoStatus::Ok,.value=GroupInviteResult{.joined=true,.alreadyIn=false,.groupId=groupId,.targetAccountId=targetAccountId}};
}

storage::RepoValueResult<im::GroupDissolveResult> im::GroupService::dissolveGroup(const std::string& groupId,const std::string& operatorAccountId,int64_t nowMs){
    if(groupId.empty()||operatorAccountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
     if(!groupRepo_){
        return {.status=storage::RepoStatus::Internal,.message="groupRepo  is not avaiable"};
    }
    //查看群状态
    auto resultGroup=groupRepo_->findGroupById(groupId);
    if(!resultGroup.ok()){
        return {.status=resultGroup.status,.message=resultGroup.message};
    }
    if(!resultGroup.value.has_value()){
        return {.status=storage::RepoStatus::NotFound};
    }
    if(resultGroup.value.value().status==storage::GroupStatus::Dissolved){
        return {.status=storage::RepoStatus::Ok,.message="The group has been dissolved"};
    }
    if(resultGroup.value.value().ownerAccountId!=operatorAccountId){
        return {.status=storage::RepoStatus::NoPermission};
    }

    auto resultDis=groupRepo_->dissolveGroup(groupId,operatorAccountId,nowMs);
    if(!resultDis.ok()){
        return {.status=resultDis.status,.message=resultDis.message};
    }
    if(!resultDis.value.has_value()){
        return {.status=storage::RepoStatus::Internal};
    }
    //获取解散成员列表
    GroupDissolveResult dissolveResult;
    dissolveResult.affectedAccountIds=resultDis.value.value().memberAccountIds;
    //同步内存
    if(!groupManager_.removeGroup(groupId)){
        LOG_WARN("Failed to remove the group in the memory");
    }
    dissolveResult.dissolved=true;
    return {.status=storage::RepoStatus::Ok,.value=dissolveResult};
}
