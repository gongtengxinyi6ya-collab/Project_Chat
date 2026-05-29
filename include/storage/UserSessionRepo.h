#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include "storage/RepoResult.h"

/*持久化登录token于用户之间的关系*/

namespace storage{
struct StoredUserSession
{
    uint64_t userId{0};//用户稳定ID
    std::string accountId{};//账号唯一标识
    std::string username{};//用户名
    std::string tokenHash{};//数据库存储的token hash
    int64_t expireAtMs{0};//过期时间
    int64_t createAtMs{0};//创建时间
    int64_t lastSeenAtMs{0};//最近使用时间
    bool revoked{false};//是否已退出或者注销
    std::optional<int64_t> revokedAtMs{std::nullopt};//token被注销时间
};
class UserSessionRepo{
public:
    virtual ~UserSessionRepo()=default;
    virtual RepoResult createSession(const StoredUserSession& session)=0;//登录成功后调用，保存token会话到数据库
    virtual std::optional<StoredUserSession> findByTokenHash(const std::string& tokenHash)=0;//TOKEN_LOGIN_REQ后调用，根据hash查询session
    virtual RepoResult touchSession(const std::string& tokenHash,int64_t lastSeenAtMs)=0;//token登录成功后更新最近使用时间
    virtual RepoResult revokeSession(const std::string& tokenHash,int64_t revokedAtMs)=0;//注销后软失效token
};

}