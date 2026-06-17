#include "storage/GroupJoinRequestRepo.h"
#include <memory>
namespace storage{
    class SqlConnectionPool;

class SqlGroupJoinRequestRepo:public GroupJoinRequestRepo{
public:
    explicit SqlGroupJoinRequestRepo(std::shared_ptr<SqlConnectionPool> pool);
    
    RepoValueResult<GroupJoinApplyResult> submit(const std::string& groupId,const std::string& applicantAAccountId,const std::string& requestMessage,int64_t nowMs)override;//提交入群申请
    RepoValueResult<std::vector<GroupJoinRequestRecord>> listPending(const std::string& groupId,size_t limit)override;//列出待审批申请
    RepoValueResult<GroupJoinReviewResult> review(const std::string&groupId,const std::string&applicationAccountId,const std::string& reviewAccountId,bool approve,size_t maxGroupMembers,int64_t nowMs)override;//审批申请
    
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}