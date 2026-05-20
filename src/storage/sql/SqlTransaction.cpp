#include "storage/sql/SqlTransaction.h"
#include "storage/sql/SqlConnection.h"
#include "logger/LogMacros.h"
storage::SqlTransaction::SqlTransaction(SqlConnection& conn):
conn_(conn){
    auto result=conn_.beginTransaction();//开启事务
    if(result.ok()){
        //事务开启成功
        active_=true;
    }
    else{//事务开启失败
        throw std::runtime_error("failed to begin transaction");
    }
}
storage::SqlTransaction::~SqlTransaction(){
    if(active_){
        auto result=conn_.rollback();
        if(!result.ok()){
            LOG_WARN("Failed to rollback transaction");
        }
    }
}
void storage::SqlTransaction::commit(){
    if(!active_){
        return;
    }
    auto result=conn_.commit();
    if(result.ok()){
        active_=false;
    }
    else{
        throw std::runtime_error("Faile to commit transaction");
    }

}
void storage::SqlTransaction::rollback(){
    if(!active_){
        return;
    }
    auto result=conn_.rollback();
    if(result.ok()){
        active_=false;
    }
    else{
        throw std::runtime_error("Failed to rollback transaction");
    }
}