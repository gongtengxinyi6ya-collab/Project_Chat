#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnection.h"

namespace storage{
SqlConnectionPool::SqlConnectionPool(const DatabaseConfig& config)
:config_(config){
}
bool SqlConnectionPool::start(){
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
void SqlConnectionPool::stop(){
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

SqlConnectionGuard SqlConnectionPool::acquire(){
    return acquireFor(acquireTimeout_);
}
size_t SqlConnectionPool::size()const{
    std::lock_guard lk(mutex_);
    return connections_.size();
}
bool SqlConnectionPool::healthy(){

    std::vector<std::shared_ptr<SqlConnection>> snapshot;
    {
        std::lock_guard lk(mutex_);
        snapshot=connections_;
    }
    for(auto& conn:snapshot){
        if(!conn||!conn->ping()){
            return false;
        }
    }
    return true;
}
void SqlConnectionPool::release(std::shared_ptr<SqlConnection> conn){
    if(!conn){
        return;
    }
    //锁外重置状态安全
    bool reusable=conn->resetSessionStateSafe();
    
    if(!started_){
        return;
    }
    if(reusable&&!conn->broken()){
        //连接可复用，加锁放回idle_
        std::lock_guard lk(mutex_);
        idle_.push(std::move(conn));
    }
    else{
        //尝试replace
        replaceConnection(conn);
    }
    
    cv_.notify_one();
}

SqlConnectionGuard SqlConnectionPool::acquireFor(std::chrono::milliseconds timeout){
    std::unique_lock lk(mutex_);//加锁等待
    bool ready=cv_.wait_for(lk,timeout,[this](){return !idle_.empty()||!started_;});
    if(!ready){//超时返回空
        acquireTimeouts_.fetch_add(1,std::memory_order_relaxed);
        return SqlConnectionGuard(*this,nullptr);
    }
    if(!started_){
        return SqlConnectionGuard(*this,nullptr);
    }
    auto conn=idle_.front();
    idle_.pop();
    SqlConnectionGuard guard(*this,conn);
    acquireCount_.fetch_add(1,std::memory_order_relaxed);
    return guard;
}
SqlConnectionPoolStats SqlConnectionPool::stats() const{
    std::lock_guard lk(mutex_);
    auto total=connections_.size();
    auto idleCount=idle_.size();
    return {.total=total,.idle=idleCount,.inUse=total-idleCount,.acquireTimeouts=acquireTimeouts_,.reconnects=reconnects_,.replaceFailures=replaceFailures_,.acquireCount=acquireCount_};
}
bool SqlConnectionPool::replaceConnection(const std::shared_ptr<SqlConnection>& oldConn){
    if(!oldConn){
        return false;
    }
    //创建新连接
    auto newConn=std::make_shared<SqlConnection>(config_);
    if(!newConn->connect()){
        return false;
    }
    //成功后替换旧连接
    {
        std::lock_guard lk(mutex_);
        if(!started_){
            return false;
        }
        for(auto& conn:connections_){
            if(conn==oldConn){
                conn=newConn;
                idle_.push(newConn);
                cv_.notify_one();
                reconnects_.fetch_add(1,std::memory_order_relaxed);
                return true;
            }
        }
    }
    //替换失败
    replaceFailures_.fetch_add(1,std::memory_order_relaxed);
    return false;
}
}