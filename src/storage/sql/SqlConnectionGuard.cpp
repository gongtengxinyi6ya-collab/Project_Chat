#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnectionPool.h"

storage::SqlConnectionGuard::SqlConnectionGuard(SqlConnectionPool& pool,std::shared_ptr<SqlConnection> conn)
:pool_(&pool),conn_(std::move(conn)){

}
storage::SqlConnectionGuard::~SqlConnectionGuard(){
    if(pool_&&conn_){
        pool_->release(std::move(conn_));
    }
}
storage::SqlConnection* storage::SqlConnectionGuard::operator->(){
    return conn_.get();
}
storage::SqlConnection& storage::SqlConnectionGuard::operator*(){
    return *conn_;
}
storage::SqlConnectionGuard::operator bool()const{
    return conn_!=nullptr;
}
storage::SqlConnectionGuard::SqlConnectionGuard(SqlConnectionGuard&& other)noexcept
:pool_(other.pool_),conn_(std::move(other.conn_)){
    other.pool_=nullptr;
}
storage::SqlConnectionGuard& storage::SqlConnectionGuard::operator=(SqlConnectionGuard&& other)noexcept{
    if(this!=&other){
        if(pool_&&conn_){//先释放当前资源防止泄露
            pool_->release(std::move(conn_));

        }
        //接管资源
        pool_=other.pool_;
        conn_=std::move(other.conn_);
        other.pool_=nullptr;
    }
    return *this;
}