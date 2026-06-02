#include "storage/sql/SqlConnection.h"
storage::SqlConnection::SqlConnection(const DatabaseConfig& config):config_(config){

}
storage::SqlConnection::~SqlConnection(){
    close();
}
bool storage::SqlConnection::connect(){
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
void storage::SqlConnection::close(){
    connected_=false;
    if(conn_){
        conn_->close();
    }
}
bool storage::SqlConnection::ping(){
    if(!connected_||!conn_){
        return false;
    }
    auto result=query("SELECT 1");
    if(result.ok()){
        return true;
    }
    return false;
}
storage::SqlResult storage::SqlConnection::execute(const std::string& sql){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
        uint64_t affected=stmt->executeUpdate(sql);
        return SqlResult{.success=true,.affectedRows=affected};
    }catch(const sql::SQLException& e){
        return SqlResult{.success=false,.error=e.what()};
    }
}
storage::SqlResult storage::SqlConnection::query(const std::string& sql){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
        auto ResultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery(sql));
        return readResultSet(ResultSet.get());
    }catch(const sql::SQLException& e){
        return SqlResult{.success=false,.error=e.what()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
bool storage::SqlConnection::connected()const{
    return connected_;
}
storage::SqlResult storage::SqlConnection::executePrepared(const std::string& sql,const std::vector<SqlParam>& params){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        auto stmt=std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sql));
        for(size_t i=0;i<params.size();i++){
            params[i].bind(stmt.get(),static_cast<int>(i+1));
        }
        uint64_t affectedRows=stmt->executeUpdate();
        return SqlResult{.success=true,.affectedRows=affectedRows};
    }catch(const sql::SQLException& e){
        return SqlResult{.success=false,.error=e.what()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
uint64_t storage::SqlConnection::fetchLastInsertId(){
    auto stmt=std::unique_ptr<sql::Statement>(conn_->createStatement());
    //执行查询上一次插入id
    auto resultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
    if(!resultSet->next()){
        throw std::runtime_error("LAST_INSERT_ID() returned no row");
    }
    //读取结果集第一行
    return resultSet->getUInt64(1);
}
storage::SqlResult storage::SqlConnection::executePreParedInsert(const std::string& sql,const std::vector<SqlParam>& params){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
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
        return SqlResult{.success=false,.error=e.what()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
storage::SqlResult storage::SqlConnection::queryPrepared(const std::string& sql,const std::vector<SqlParam>& params){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not conncted"};
    }
    try{
        auto stmt=std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sql));
        for(size_t i=0;i<params.size();i++){
            params[i].bind(stmt.get(),static_cast<int>(i+1));
        }
        auto ResultSet=std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        return readResultSet(ResultSet.get());
    }catch(const sql::SQLException& e){
        return SqlResult{.success=false,.error=e.what()};
    }
    return SqlResult{.success=false,.error="unknown error"};
}
storage::SqlResult storage::SqlConnection::beginTransaction(){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        conn_->setAutoCommit(false);//关闭自动提交
        inTransaction_=true;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        return SqlResult{.success=false,.error=e.what()};
    }
}
storage::SqlResult storage::SqlConnection::commit(){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        conn_->commit();
        conn_->setAutoCommit(true);
        inTransaction_=false;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        try{
            conn_->setAutoCommit(true);
        }catch(const sql::SQLException& e2){
            // Ignore this error
        }
        return SqlResult{.success=false,.error=e.what()};
    }
}
storage::SqlResult storage::SqlConnection::rollback(){
    if(!connected_||!conn_){
        return SqlResult{.success=false,.error="not connected"};
    }
    try{
        conn_->rollback();
        conn_->setAutoCommit(true);//恢复自动提交
        inTransaction_=false;
        return SqlResult{.success=true};
    }catch(const sql::SQLException& e){
        try{
            conn_->setAutoCommit(true);
        }catch(...){

        }
        return SqlResult{.success=false,.error=e.what()};
    }
}
bool storage::SqlConnection::inTransaction()const{
    return inTransaction_;
}
void storage::SqlConnection::resetSessionState(){
    if(inTransaction_){
        rollback();
    }
    conn_->setAutoCommit(true);
}
storage::SqlResult storage::SqlConnection::readResultSet(sql::ResultSet* resultset){
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