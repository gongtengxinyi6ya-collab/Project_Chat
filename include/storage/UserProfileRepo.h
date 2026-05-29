#pragma once 
#include <string>
#include <cstdint>
#include <optional>
#include "storage/RepoResult.h"

/*屏蔽资料数据具体存储实现*/
namespace storage{
/*数据库中用户的公开基础资料*/
struct UserProfile{
    uint64_t userId{0};//关联users.id的稳定主键
    std::string username{};//登录账号，用于响应中标识用户
    std::string nickname{};//昵称
    std::string avatarUrl{};//头像资源地址
    std::string signature{};//个性签名
    int64_t updateAtMs{0};//最近资料更新时间

};

class UserProfileRepo{
public:
    virtual ~UserProfileRepo()=default;//支持多态销毁
    virtual RepoResult createDefaultProfile(uint64_t userId,const std::string& username)=0;//注册后生成默认资料
    virtual std::optional<UserProfile> findByUserId(uint64_t userId)const=0;//按当前登录用户ID查询资料
    virtual std::optional<UserProfile> findByUsername(const std::string& username)const=0;//按账号名查询资料
    virtual RepoResult updateProfile(uint64_t userId,const std::string& nickname,const std::string& avatarUrl,const std::string& signature,int64_t updateAtMs)=0;//更新自己资料
};
}