#include "storage/sql/SqlErrorMapper.h"

namespace storage{

RepoStatus mapSqlErrorToRepoStatus(const SqlResult& result){
    if(result.ok()){
        return RepoStatus::Ok;
    }
    if(result.errorCode==1062){
        return RepoStatus::AlreadyExists;
    }
    if(result.errorCode==1452){
        return RepoStatus::NotFound;
    }
    if(result.errorCode==1451){
        return RepoStatus::Conflict;
    }
    return RepoStatus::SqlError;
}
std::string formatSqlError(const SqlResult& result){
    return "SQL error: code="+std::to_string(result.errorCode)+",state="+result.sqlState+",msg="+result.error;
}
}