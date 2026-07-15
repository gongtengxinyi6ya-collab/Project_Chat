#include "storage/sql/SqlConnection.h"

namespace storage{
SqlConnection::SqlConnection(const DatabaseConfig& config):config_(config){

}
SqlConnection::~SqlConnection(){
    close();
}
bool SqlConnection::connect(){
    try{
        driver_=sql::mysql::get_mysql_driver_instance();
        std::string url="tcp://"+config_.host()+":"+std::to_string(config_.port());
        conn_.reset(driver_->connect(url,config_.user(),config_.password()));//设置连接
        conn_->setSchema(config_.database());
    }catch(const sql::SQLException& e){
        std::cerr << "Error connecting to database: " << e.what() << std::endl;
        connected_=false;
        return false;
    }
    connected_=true;
    return true;
}
void SqlConnection::close(){
    connected_=false;
    if(conn_){
        conn_->close();
    }
}
bool SqlConnection::ping(){
    if(!connected_||!conn_){
        return false;
    }
    auto result=query("SELECT 1");
    if(result.ok()){
        return true;
    }
    return false;
}
SqlResult SqlConnection::execute(const std::string& sql){
    if(!ensureConnected()){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
        uint64_t affected=stmt->executeUpdate(sql);
        return SqlResult{.success=true,.affectedRows=affected};
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
}
SqlResult SqlConnection::query(const std::string& sql){
    if(!ensureConnected()){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
        auto ResultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery(sql));
        return readResultSet(ResultSet.get());
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
bool SqlConnection::connected()const{
    return connected_;
}
SqlResult SqlConnection::executePrepared(const std::string& sql,const std::vector<SqlParam>& params){
    if(!ensureConnected()){
        return {.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sql));
        for(size_t i=0;i<params.size();i++){
            params[i].bind(stmt.get(),static_cast<int>(i+1));
        }
        uint64_t affectedRows=stmt->executeUpdate();
        return SqlResult{.success=true,.affectedRows=affectedRows};
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
uint64_t SqlConnection::fetchLastInsertId(){
    auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
    //执行查询上一次插入id
    auto resultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
    if(!resultSet->next()){
        throw std::runtime_error("LAST_INSERT_ID() returned no row");
    }
    //读取结果集第一行
    return resultSet->getUInt64(1);
}
SqlResult SqlConnection::executePreParedInsert(const std::string& sql,const std::vector<SqlParam>& params){
    if(!ensureConnected()){
        return {.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sql));
        for(size_t i=0;i<params.size();i++){
            params[i].bind(stmt.get(),static_cast<int>(i+1));
        }
        uint64_t affectedRows=stmt->executeUpdate();
        if(affectedRows==0){
            return SqlResult{.success=true,.affectedRows=0,.lastInsertId=0};
        }
        uint64_t lastInsertId=fetchLastInsertId();
        return SqlResult{.success=true,.affectedRows=affectedRows,.lastInsertId=lastInsertId};
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
SqlResult SqlConnection::queryPrepared(const std::string& sql,const std::vector<SqlParam>& params){
    if(!ensureConnected()){
        return {.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sql));
        for(size_t i=0;i<params.size();i++){
            params[i].bind(stmt.get(),static_cast<int>(i+1));
        }
        auto ResultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        return readResultSet(ResultSet.get());
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
SqlResult SqlConnection::beginTransaction(){
    if(!ensureConnected()){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        inTransaction_=true;
        autoCommit_=false;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        if (isConnectionError(e)) {
            markBroken();
        }
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
}
SqlResult SqlConnection::commit(){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        conn_->commit();
        inTransaction_=false;
        autoCommit_=true;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        autoCommit_=true;        
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
}
SqlResult SqlConnection::rollback(){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        conn_->rollback();
        autoCommit_=true;
        inTransaction_=false;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        autoCommit_=true;
        return SqlResult{.success=false,.error=e.what(),.errorCode=e.getErrorCode(),.sqlState=e.getSQLState()};
    }
}
bool SqlConnection::inTransaction()const{
    return inTransaction_;
}
void SqlConnection::resetSessionState(){
    if(inTransaction_){
        rollback();
    }
    else if(autoCommit_){
        return ;
    }
    else{
        conn_->setAutoCommit(true);
    }
}
SqlResult SqlConnection::readResultSet(sql::ResultSet* resultset){
    SqlResult result;
    while(resultset->next()){
        SqlRow row;
        auto meta=resultset->getMetaData();
            for(size_t i=1;i<=meta->getColumnCount();++i){
                std::string columnName=meta->getColumnLabel(i);
                std::string value=resultset->getString(i);
                row[columnName]=value;
            }
           
            result.rows.push_back(std::move(row));
    }
    return result;
}

bool SqlConnection::ensureConnected(){
    if(connected_&&conn_&&!broken_){
        return true;
    }
    else{
        if(reconnect()){
            return true;
        }
    }
    return false;
}
bool SqlConnection::reconnect(){
    //清理旧连接
    close();
    if(connect()){
        broken_=false;
        reconnectCount_++;
        return true;
    }
    broken_=true;
    return false;
}
bool SqlConnection::resetSessionStateSafe(){
    try{
        //若处于事务中，进行回滚
        if(inTransaction_){
            auto result=rollback();
            if(!result.ok()){
                broken_=true;
                return false;
            }
            return true;
        }
        conn_->setAutoCommit(true);
        return true;
    }catch(const sql::SQLException& e){
        try{
            conn_->setAutoCommit(true);
        }catch(...){

        }
        if (isConnectionError(e)) {
            markBroken();
        }
        return false;
    }
}
void SqlConnection::markBroken(){
    broken_=true;
    connected_=false;
}
bool SqlConnection::broken() const{
    return broken_;
}
bool SqlConnection::isConnectionError(const sql::SQLException&e)const{
    if(e.getSQLState().rfind("08")==0){
        return true;
    }
    return false;
}
}