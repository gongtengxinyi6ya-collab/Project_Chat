#include "storage/sql/SqlFriendRequestRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
storage::SqlFriendRequestRepo::SqlFriendRequestRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}


storage::RepoValueResult<uint64_t> storage::SqlFriendRequestRepo::createPendingRequest(const std::string& requester,const std::string& receiver,int64_t nowMs){
    if(requester.empty()||receiver.empty()){
        return RepoValueResult{};
    }
}