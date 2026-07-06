#pragma once
#include <memory>
#include "RepositoryBundle.h"
#include "config/DatabaseConfig.h"
#include "config/AppConfig.h"

/*纯静态工具类，根据配置创建一组Repo,隐藏Memory/SQL创建细节*/
namespace storage{
class RepositoryFactory{
public:
    static RepositoryBundle createMemory();//创建内存版Repository
    static RepositoryBundle createSql(const DatabaseConfig& databaseConfig);//创建SQL版Repository
    static RepositoryBundle create(const AppConfig& config);//根据总配置选择Memory或SQL
};
}