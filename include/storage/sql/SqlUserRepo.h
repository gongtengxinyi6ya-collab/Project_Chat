#include <memory>
#include <string>
#include "SqlConnectionPool.h"
#include "UserRepo.h"

/*用SQL实现UserRepo
辅助用户创建和用户存在性查询*/

namespace storage{
class SqlUserRepo:public UserRepo{
public:
    explicit SqlUserRepo(std::shared_ptr<SqlConnectionPool> pool);
    RepoResult createUser(const std::string& username)override;//创建用户
    bool userExists(const std::string& username)override;//判断用户是否存在
private:
    std::shared_ptr<SqlConnectionPool> pool_;//从连接池获取连接，每次操作通过SqlConnectionGuard执行SQL
};
}