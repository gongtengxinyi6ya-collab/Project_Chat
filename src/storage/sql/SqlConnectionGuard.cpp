#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnectionPool.h"

storage::SqlConnectionGuard::SqlConnectionGuard(SqlConnectionPool& pool,std::shared_ptr<SqlConnection> conn)
:pool_(&pool),conn_(conn){

}
storage::SqlConnectionGuard::~SqlConnectionGuard(){

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