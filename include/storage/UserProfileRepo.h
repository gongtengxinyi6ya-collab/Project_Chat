#pragma once 
#include <string>
#include <cstdint>
#include <optional>
#include "storage/RepoResult.h"
#include <vector>
#include "types/UserTypes.h"
/*屏蔽资料数据具体存储实现*/
namespace storage{

class UserProfileRepo{
public:
    virtual ~UserProfileRepo()=default;//支持多态销毁
    virtual RepoResult createDefaultProfile(uint64_t userId,const std::string&accountId,const std::string& username)=0;//注册后生成默认资料
    virtual std::optional<UserProfile> findByUserId(uint64_t userId)const=0;//按当前登录用户ID查询资料
    virtual std::optional<UserProfile> findByUsername(const std::string& username)const=0;//按账号名查询资料
    virtual std::vector<UserProfile> findByAccountIds(const std::vector<std::string>& accountIds)const=0;//批量查询账号
    virtual std::optional<UserProfile> findByAccountId(const std::string& accountId)const=0;//单用户精确查询
    virtual RepoResult updateProfile(uint64_t userId,const std::string& nickname,const std::string& avatarUrl,const std::string& signature,int64_t updateAtMs)=0;//更新自己资料
};
}