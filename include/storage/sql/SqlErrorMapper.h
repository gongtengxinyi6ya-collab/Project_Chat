#pragma once
#include <string>
#include "storage/RepoResult.h"
#include "storage/sql/SqlResult.h"
namespace storage{
    RepoStatus mapSqlErrorToRepoStatus(const SqlResult& result);//映射sql错误
    std::string formatSqlError(const SqlResult& result);
}