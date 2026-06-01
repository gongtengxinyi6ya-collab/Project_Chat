#pragma once
#include <memory>
#include "storage/FriendRepo.h"
/*实现抽象类FriendRepo*/
namespace storage{
    class SqlConnectionPool;

class SqlFriendRepo:public FriendRepo{
public:
    explicit SqlFriendRepo(std::shared_ptr<SqlConnectionPool> pool);//保存连接池引用
    RepoResult addFriendPair(const std::string& accountId,const std::string& friendAccountId,int64_t createAtMs)override;
    RepoResult removeFriendPair(const std::string& accountId,const std::string&friendAccountId)override;//删除好友
    bool areFriends(const std::string& accountId,const std::string& friendAccountId)const override; //查询好友关系是否有效
    std::vector<std::string> listFriendccountIds(const std::string& accountId)const override;//查询好友账号列表

private:
    std::shared_ptr<SqlConnectionPool> pool_;//复用现有的MySQL连接池
};
}