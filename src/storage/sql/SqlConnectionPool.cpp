#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"

storage::SqlConnectionPool::SqlConnectionPool(const DatabaseConfig& config)
:config_(config){
}
bool storage::SqlConnectionPool::start(){
    std::lock_guard lock(mutex_);
    if(started_){
        return true;
    }
    for(size_t i=0;i<config_.poolSize();++i){
        auto conn=std::make_shared<SqlConnection>(config_);
        if(!conn->connect()){
            return false;
        }
        idle_.push(conn);
    }
    started_=true;
    return true;
}
std::