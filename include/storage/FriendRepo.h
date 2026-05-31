#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"

/*抽象接口：负责持久化好友关系*/
namespace storage{

enum class FriendRelationStatus:uint8_t{//好友关系状态
    Actice=1,
    Deleted=2

};

struct FriendRelation{
    std::string accountId{};//关系所属账号
    std::string friendAccountId{};//好友账号
    int64_t createAtMs{0};//添加好友时间
    FriendRelationStatus status{FriendRelationStatus::Actice};

};

class FriendRepo{
public:
    virtual RepoResult addFriendPair(const std::string& accountId,const std::string&friendAccountId,int64_t createAtMs)=0;//添加好友
    virtual RepoResult removeFriendPair(const std::string& accountId,const std::string&friendAccountId)=0;//删除好友
    virtual bool areFriends(const std::string& accountId,const std::string& friendAccountId)const=0;//查询好友关系是否有效
    virtual std::vector<std::string> listFriendccountIds(const std::string& accountId)const =0;//查询好友账号列表
};
}