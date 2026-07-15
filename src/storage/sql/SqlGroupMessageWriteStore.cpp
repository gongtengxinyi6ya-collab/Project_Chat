#include "storage/sql/SqlGroupMessageWriteStore.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlTransaction.h"
#include <exception>
namespace storage{
SqlGroupMessageWriteStore::GroupMessageWriteStore(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){
}

RepoValueResult<std::uint64_t> SqlGroupMessageWriteStore::commit(const im::GroupMessageWriteCommand& command){
    if(command)
}
}