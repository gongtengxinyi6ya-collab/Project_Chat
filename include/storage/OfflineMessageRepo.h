#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <storage/RepoResult.h>
/*抽象离线消息索引存储
保存消息用户名，消息ID，群ID，无需保存正文*/

namespace storage{
//枚举区分离线索引指向群消息还是私聊消息
enum class OfflineMessageType:uint8_t{
    Group=1,
    Direct=2
};
struct OfflineMessageIndex{
    uint64_t msgId{0};//离线的消息ID
    OfflineMessageType type{OfflineMessageType::Group};
    std::string groupId{};//群消息所属群，私聊时为空
    std::string peerAccountId{};//私聊对端账号
};

class OfflineMessageRepo{
public:
    virtual RepoResult saveOfflineMessage(const std::string& accountId,uint64_t msgId,const std::string& groupId)=0;//保存一条群离线消息索引
    virtual RepoResult saveOfflineDirectMessage(const std::string& accountId,uint64_t msgId,const std::string& peerAccountId)=0;//保存一条私聊离线消息索引
    virtual std::vector<OfflineMessageIndex> listOfflineMessage(const std::string& accountId,size_t limit)=0;//查询用户的离线消息索引
    virtual RepoResult ackOfflineMessage(const std::string& accountId,const std::vector<uint64_t>& msgId)=0;//客户端确认后删除离线消息索引
};
inline std::string offlineMessageTypeToString(OfflineMessageType type){
    switch(type){
        case OfflineMessageType::Group:
            return "Group";
        case OfflineMessageType::Direct:
            return "Direct";
    }
}
}