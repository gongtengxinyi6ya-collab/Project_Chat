#pragma once
#include <memory>
#include "storage/UserSessionRepo.h"

namespace storage{
class SqlConnectionPool;

class SqlUserSessionRepo:public UserSessionRepo{
public:
    explicit SqlUserSessionRepo(std::shared_ptr<SqlConnectionPool> pool);
    RepoResult createSession(const StoredUserSession& session)override;//登录成功后调用，保存到数据库
    std::optional<StoredUserSession> findByTokenHash(const std::string& tokenHash)override;//TOKEN_LOGIN_REQ后调用，根据hash查询session
    RepoResult touchSession(const std::string& tokenHash,int64_t lastSeenAtMs)override;//token登录成功后更新最近使用时间
    RepoResult revokeSession(const std::string& tokenHash)override;//退出登录后删除session
private:
    std::shared_ptr<SqlConnectionPool> pool_;
};
}