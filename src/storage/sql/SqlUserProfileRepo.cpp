#include "storage/sql/SqlUserProfileRepo.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include <chrono>

storage::SqlUserProfileRepo::SqlUserProfileRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}
storage::RepoResult storage::SqlUserProfileRepo::createDefaultProfile(uint64_t userId,const std::string& username){
    if(username.empty()||userId==0){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    //默认昵称使用username
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto result=conn->executePrepared("INSERT INTO user_profiles (user_id,username,nickname,avatar_url,signature,updated_at_ms) VALUES(?,?,?,'','',?)",{userId,username,username,nowMs});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        if(result.error.find("Duplicate entry")!=std::string::npos){//唯一键冲突
            return RepoResult{.status=RepoStatus::AlreadyExists,.message="User already exists"};
        }
         return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to create userProfile"};
}
std::optional<storage::UserProfile> storage::SqlUserProfileRepo::findByUserId(uint64_t userId)const{
    if(userId==0){
        return std::nullopt;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return std::nullopt;
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT user_id,username,nickname,avatar_url,signature,updated_at_ms FROM user_profiles WHERE user_id=? LIMIT 1",{userId});
        if(result.ok()&&!result.rows.empty()){
            const auto& row=result.rows.front();
            UserProfile profile;
            auto userIdPair=row.find("user_id");
            profile.userId=userIdPair!=row.end()?std::stoull(userIdPair->second):0;
            auto usernamePair=row.find("username");
            profile.username=usernamePair!=row.end()?usernamePair->second:"";
            auto nicknamePair=row.find("nickname");
            profile.nickname=nicknamePair!=row.end()?nicknamePair->second:"";
            auto avatarUrlPair=row.find("avatar_url");
            profile.avatarUrl=avatarUrlPair!=row.end()?avatarUrlPair->second:"";
            auto signaturePair=row.find("signature");
            profile.signature=signaturePair!=row.end()?signaturePair->second:"";
            auto updatedAtPair=row.find("updated_at_ms");
            profile.updateAtMs=updatedAtPair!=row.end()?std::stoll(updatedAtPair->second):0;
            return profile;
        }
    }
    return std::nullopt;
}
std::optional<storage::UserProfile> storage::SqlUserProfileRepo::findByUsername(const std::string& username)const{
    if(username.empty()){
        return std::nullopt;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return std::nullopt;
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT user_id,username,nickname,avatar_url,signature,updated_at_ms FROM user_profiles WHERE username=? LIMIT 1",{username});
        if(result.ok()&&!result.rows.empty()){
            const auto& row=result.rows.front();
            UserProfile profile;
            auto userIdPair=row.find("user_id");
            profile.userId=userIdPair!=row.end()?std::stoull(userIdPair->second):0;
            auto usernamePair=row.find("username");
            profile.username=usernamePair!=row.end()?usernamePair->second:"";
            auto nicknamePair=row.find("nickname");
            profile.nickname=nicknamePair!=row.end()?nicknamePair->second:"";
            auto avatarUrlPair=row.find("avatar_url");
            profile.avatarUrl=avatarUrlPair!=row.end()?avatarUrlPair->second:"";
            auto signaturePair=row.find("signature");
            profile.signature=signaturePair!=row.end()?signaturePair->second:"";
            auto updatedAtPair=row.find("updated_at_ms");
            profile.updateAtMs=updatedAtPair!=row.end()?std::stoll(updatedAtPair->second):0;
            return profile;
        }
    }
    return std::nullopt;
}

storage::RepoResult storage::SqlUserProfileRepo::updateProfile(uint64_t userId,const std::string& nickname,const std::string& avatarUrl,const std::string& signature,int64_t updateAtMs){
    if(userId==0||nickname.empty()||updateAtMs==0){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    //默认昵称使用username
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto result=conn->executePrepared("UPDATE user_profiles SET nickname=?,avatar_url=?,signature=?,updated_at_ms=? WHERE user_id=?",{nickname,avatarUrl,signature,updateAtMs,userId});
        if(result.ok()&&result.affectedRows!=0){
            return RepoResult{.status=RepoStatus::Ok};
        }
        if(!result.ok()){
            return RepoResult{.status=RepoStatus::NotFound,.message="Failed to update profile"};
        }
         return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to update userProfile"};
}