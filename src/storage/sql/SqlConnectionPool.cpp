#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"

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
        connections_.push_back(conn);
    }
    started_=true;

    return true;
}
void storage::SqlConnectionPool::stop(){
    std::lock_guard lk(mutex_);
    for(auto& conn:connections_){
        conn->close();
    }
    while(!idle_.empty()){
        idle_.pop();
    }
    connections_.clear();
    started_=false;
    cv_.notify_all();
}

storage::SqlConnectionGuard storage::SqlConnectionPool::acquire(){
    std::unique_lock lk(mutex_);
    cv_.wait(lk,[this](){return !idle_.empty()||!started_;});
    if(!started_){
        return SqlConnectionGuard(*this,nullptr);
    }
    auto conn=idle_.front();
    idle_.pop();
    SqlConnectionGuard guard(*this,conn);
    return guard;
}
size_t storage::SqlConnectionPool::size()const{
    std::lock_guard lk(mutex_);
    return connections_.size();
}
bool storage::SqlConnectionPool::healthy(){
    std::lock_guard lk(mutex_);
    for(auto& conn:connections_){
        if(!conn->ping()){
            return false;
        }
    }
    return true;
}
void storage::SqlConnectionPool::release(std::shared_ptr<SqlConnection> conn){
    if(!conn){
        return;
    }
    {
        std::lock_guard lk(mutex_);
        if(!started_){
            return;
        }
        idle_.push(std::move(conn));
    }
    cv_.notify_one();
}