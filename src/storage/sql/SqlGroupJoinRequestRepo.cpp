#include "storage/sql/SqLGroupJoinRequestRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include <stdexcept>
storage::RepoValueResult<storage::GroupJoinApplyResult> storage::SqlGroupJoinRequestRepo::submit(const std::string& groupId,const std::string& applicantAccountId,const std::string& requestMessage,int64_t nowMs){
    if(groupId.empty()||applicantAccountId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    //事务处理
    try{
        SqlTransaction transaction(*conn);
        //锁定目标群
        auto resultGroup=conn->queryPrepared(R"(
            SELECT owner, status
            FROM chat_groups
            WHERE group_id = ?
            AND status=0
            FOR UPDATE;
            )"
            ,{groupId});
        if(!resultGroup.ok()){
            return {.status=RepoStatus::SqlError,.message=resultGroup.error};
        }
        //检查群存在且未解散
        if(resultGroup.rows.empty()){
            return {.status=RepoStatus::GroupNotFound,.message="group not found"};
        }
        //检查申请者是否已经是成员
        auto resultMember=conn->queryPrepared(R"(
            SELECT 1
            FROM group_members
            WHERE group_id=? AND account_id=?
            LIMIT 1 FOR UPDATE
            )",{groupId,applicantAccountId});
        if(!resultMember.ok()){
            return {.status=RepoStatus::SqlError,.message=resultMember.error};
        }
        if(!resultMember.rows.empty()){
            return {.status=RepoStatus::AlreadyExists,.message="already in the group"};
        }
        //检查是否有pending申请
        auto resultRequest=conn->queryPrepared(R"(
            SELECT id,status 
            FROM group_join_requests
            WHERE group_id=? AND applicant_account_id=?
            LIMIT 1 FOR UPDATE
            )",{groupId,applicantAccountId});
        if(!resultRequest.ok()){
            return {.status=RepoStatus::SqlError,.message=resultRequest.error};
        }
        if(!resultRequest.rows.empty()){
            auto row=resultRequest.rows.front();
            auto statusOpt=getGroupJoinRequestStatus(getUInt64(row,"status"));
            if(!statusOpt.has_value()){
                return {.status=RepoStatus::Internal,.message="status invalid"};
            }
            if(statusOpt.value()==GroupJoinRequestStatus::Pending){//已有Pending,幂等返回
                return {.status=RepoStatus::Ok,.value=GroupJoinApplyResult{.alreadyPending=true,.groupId=groupId,.applicantAccountId=applicantAccountId}};
            }
            if(statusOpt.value()==GroupJoinRequestStatus::Rejected){
                //被拒绝则更新为Pending
                auto resultUpdate=conn->executePrepared(R"(
                    UPDATE group_join_requests
                    SET status=0,
                        request_message=?,
                        reviewer_account_id=?,
                        created_at_ms=?,
                        reviewed_at_ms=0
                        WHERE id=?
                    )",{requestMessage,"",nowMs,getUInt64(row,"id")});
                if(!resultUpdate.ok()){
                    return {.status=RepoStatus::SqlError,.message=resultUpdate.error};
                }
                transaction.commit();
                return {.status=RepoStatus::Ok,.value=GroupJoinApplyResult{.submitted=true,.groupId=groupId,.applicantAccountId=applicantAccountId}};
            }
            //创建申请
            auto resultInsert=conn->executePrepared(R"(
                INSERT INTO group_join_requests(
                group_id,
                applicant_account_id,
                status,
                request_message,
                reviewer_account_id,
                created_at_ms,
                reviewed_at_ms
            )
            VALUES(?, ?, 0, ?, '', ?, 0)
                )",{groupId,applicantAccountId,requestMessage,nowMs});
            if(!resultInsert.ok()){
                return {.status=RepoStatus::SqlError,.message=resultInsert.error};
            }

            transaction.commit();
            return {.status=RepoStatus::Ok,.value=GroupJoinApplyResult{.submitted=true,.groupId=groupId,.applicantAccountId=applicantAccountId}};
            
        }
    }catch(const std::exception& e){
        return {.status=RepoStatus::SqlError,.message=e.what()};
    }
    
}
storage::RepoValueResult<std::vector<storage::GroupJoinRequestRecord>> storage::SqlGroupJoinRequestRepo::listPending(const std::string& groupId,size_t limit){
    if(groupId.empty()||limit==0){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    auto result=conn->queryPrepared()
}
storage::RepoValueResult<storage::GroupJoinReviewResult> storage::SqlGroupJoinRequestRepo::review(const std::string&groupId,const std::string&applicationAccountId,const std::string& reviewAccountId,bool approve,size_t maxGroupMembers,int64_t nowMs){

}
    