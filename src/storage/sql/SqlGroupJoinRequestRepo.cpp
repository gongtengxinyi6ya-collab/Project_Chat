#include "storage/sql/SqLGroupJoinRequestRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
RepoValueResult<GroupJoinApplyResult> storage::SqlGroupJoinRequestRepo::submit(const std::string& groupId,const std::string& applicantAAccountId,const std::string& requestMessage,int64_t nowMs){
    if(groupId.empty||applicantAAccountId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the Database"};
    }
    
}
RepoValueResult<std::vector<GroupJoinRequestRecord>> storage::SqlGroupJoinRequestRepo::listPending(const std::string& groupId,size_t limit){

}
RepoValueResult<GroupJoinReviewResult> storage::SqlGroupJoinRequestRepo::review(const std::string&groupId,const std::string&applicationAccountId,const std::string& reviewAccountId,bool approve,size_t maxGroupMembers,int64_t nowMs){

}
    