#include "storage/sql/SqlConnection.h"
storage::SqlConnection::SqlConnection(const DatabaseConfig& config):config_(config){

}
storage::SqlConnection::~SqlConnection(){
    connected_=false;
}
bool storage::SqlConnection::connect(){
    connected_=true;
    return true;
}
void storage::SqlConnection::close(){
    connected_=false;
}
bool storage::SqlConnection::ping(){
    return connected_;
}
storage::SqlResult storage::SqlConnection::execute(const std::string& sql){
    if(!connected_){
        return SqlResult{.success=false,.error="not connected"};
    }
    return SqlResult{.success=true};
}
storage::SqlResult storage::SqlConnection::query(const std::string& sql){
    if(!connected_){
        return SqlResult{.success=false,.error="not connected"};
    }
    return SqlResult{.success=true};
}
bool storage::SqlConnection::connected()const{
    return connected_;
}