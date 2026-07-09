#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/types/GroupTypes.h"
#include "storage/RepoValueResult.h"
/*群聊入群申请抽象*/

namespace storage{
class GroupJoinRequestRepo{
public:
    virtual ~GroupJoinRequestRepo()=default;
    virtual RepoValueResult<GroupJoinApplyResult> submit(const std::string& groupId,const std::string& applicantAccountId,const std::string& requestMessage,int64_t nowMs)=0;//提交入群申请
    virtual RepoValueResult<std::vector<GroupJoinRequestRecord>> listPending(const std::string& groupId,size_t limit)=0;//列出待审批申请
    virtual RepoValueResult<GroupJoinReviewResult> review(const std::string&groupId,const std::string&applicantAccountId,const std::string& reviewAccountId,bool approve,size_t maxGroupMembers,int64_t nowMs)=0;//审批申请
    virtual RepoValueResult<size_t> deleteHandledBefore(int64_t cutoffMs, size_t limit) = 0;//删除已审批的入群申请
};
}