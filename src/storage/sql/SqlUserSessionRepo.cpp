#include "storage/sql/SqlUserSessionRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlErrorMapper.h"
#include <chrono>
#include <optional>

namespace storage{
storage::SqlUserSessionRepo::SqlUserSessionRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){
}
storage::RepoResult storage::SqlUserSessionRepo::createSession(const storage::StoredUserSession& session){
    if(session.username.empty()||session.tokenHash.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO user_sessions(user_id,account_id,username,token_hash,expire_at_ms,created_at_ms,last_seen_at_ms,revoked) VALUES(?,?,?,?,?,?,?,0)",{session.userId,session.accountId,session.username,session.tokenHash,session.expireAtMs,session.createAtMs,session.lastSeenAtMs});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        auto status=mapSqlErrorToRepoStatus(result);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="UserSession already exiest"};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result)};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to create session"};
}
std::optional<storage::StoredUserSession> storage::SqlUserSessionRepo::findByTokenHash(const std::string& tokenHash){
    if(tokenHash.empty()){
        return std::nullopt;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return std::nullopt;
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT user_id,account_id,username,token_hash,expire_at_ms,created_at_ms,last_seen_at_ms,revoked,revoked_at_ms FROM user_sessions WHERE token_hash=? LIMIT 1",{tokenHash});
        if(result.ok()&&!result.rows.empty()){
            const auto& row=result.rows.front();
            StoredUserSession session;
            auto userIdPair=row.find("user_id");
            session.userId=userIdPair!=row.end()?std::stoull(userIdPair->second):0;
            auto accountIdPair=row.find("account_id");
            session.accountId=accountIdPair!=row.end()?accountIdPair->second:"";
            auto usernamePair=row.find("username");
            session.username=usernamePair!=row.end()?usernamePair->second:"";
            auto tokenHashPair=row.find("token_hash");
            session.tokenHash=tokenHashPair!=row.end()?tokenHashPair->second:"";    
            auto expireAtPair=row.find("expire_at_ms");
            session.expireAtMs=expireAtPair!=row.end()?std::stoll(expireAtPair->second):0;
            auto createAtPair=row.find("created_at_ms");
            session.createAtMs=createAtPair!=row.end()?std::stoll(createAtPair->second):0;
            auto lastSeenAtPair=row.find("last_seen_at_ms");
            session.lastSeenAtMs=lastSeenAtPair!=row.end()?std::stoll(lastSeenAtPair->second):0;
            auto revokedPair=row.find("revoked");
            session.revoked=revokedPair!=row.end()&&revokedPair->second=="1";
            auto revokedAtPair=row.find("revoked_at_ms");
            if(revokedAtPair!=row.end()&&revokedAtPair->second!="NULL"&&!revokedAtPair->second.empty()){
                session.revokedAtMs=std::stoll(revokedAtPair->second);
            }
            else{
                session.revokedAtMs=std::nullopt;
            }
            return session;
        }
    }
    return std::nullopt;
}
storage::RepoResult storage::SqlUserSessionRepo::touchSession(const std::string& tokenHash,int64_t lastSeenAtMs){
    if(tokenHash.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("UPDATE user_sessions SET last_seen_at_ms=? WHERE token_hash=? AND revoked=0",{lastSeenAtMs,tokenHash});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to touch session"};
}
storage::RepoResult storage::SqlUserSessionRepo::revokeSession(const std::string& tokenHash,int64_t revokedAt){
    if(tokenHash.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("UPDATE user_sessions SET revoked=1,revoked_at_ms=? WHERE token_hash=? AND revoked=0",{revokedAt,tokenHash});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to revoke session"};
}
}