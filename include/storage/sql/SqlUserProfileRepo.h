#include "storage/UserProfileRepo.h"
#include <memory>
/*基于当前连接池，实现用户资料持久化查询与修改*/
namespace storage{
class SqlConnectionPool;

class SqlUserProfileRepo:public UserProfileRepo{
public:
    explicit SqlUserProfileRepo(std::shared_ptr<SqlConnectionPool> pool);
    virtual RepoResult createDefaultProfile(uint64_t userId,const std::string& username)override;//注册后生成默认资料
    virtual std::optional<UserProfile> findByUserId(uint64_t userId)const override;//按当前登录用户ID查询资料
    virtual std::optional<UserProfile> findByUsername(const std::string& username)const override;//按账号名查询资料
    virtual RepoResult updateProfile(uint64_t userId,const std::string& nickname,const std::string& avatarUrl,const std::string& signature,int64_t updateAtMs)override;//更新自己资料

private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}