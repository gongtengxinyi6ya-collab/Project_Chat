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