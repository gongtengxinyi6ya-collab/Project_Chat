#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"
#include "storage/types/FriendTypes.h"
/*抽象接口：负责持久化好友关系*/
namespace storage{
class FriendRepo{
public:
    virtual ~FriendRepo()=default;
    virtual RepoResult addFriendPair(const std::string& accountId,const std::string&friendAccountId,int64_t createAtMs)=0;//添加好友
    virtual RepoResult removeFriendPair(const std::string& accountId,const std::string&friendAccountId)=0;//删除好友
    virtual bool areFriends(const std::string& accountId,const std::string& friendAccountId)const=0;//查询好友关系是否有效
    virtual std::vector<std::string> listFriendAccountIds(const std::string& accountId)const =0;//查询好友账号列表
};
}