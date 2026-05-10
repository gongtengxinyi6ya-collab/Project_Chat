#include "storage/sql/SqlConnection.h"
bool storage::SqlConnection::connect(){
    return true;
}
void storage::SqlConnection::close(){
    connected_=false;
}
bool storage::SqlConnection::ping(){
    return connected_;
}
SqlResult storage::SqlConnection::execute(const std::string& sql){
    return SqlResult{true};
}
SqlResult storage::SqlConnection::query(const std::string& sql){
    return SqlResult{true};
}