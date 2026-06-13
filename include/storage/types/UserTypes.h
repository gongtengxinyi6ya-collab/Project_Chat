#pragma once
#include <string>
#include <cstdint>
namespace storage{
struct UserAuthInfo
{//用户注册信息结构
    uint64_t userId{0};//用户内部Id
    std::string accountId{};//对外唯一账号ID
    std::string username{};//展示名
    std::string passwordHash{};//数据库中密码hash
    std::string passwordSalt{};//数据库中salt
    int status{0};//账号状态
};

struct UserProfile{
    //用户公开资料
    uint64_t userId{0};//关联users.id的稳定主键
    std::string accountId{};//对外唯一账号
    std::string username{};//账号名称
    std::string nickname{};//昵称
    std::string avatarUrl{};//头像资源地址
    std::string signature{};//个性签名
    int64_t updateAtMs{0};//最近资料更新时间

};
}