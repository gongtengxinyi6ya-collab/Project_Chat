#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <storage/RepoResult.h>
/*抽象离线消息索引存储
保存消息用户名，消息ID，群ID，无需保存正文*/

namespace storage{
struct OfflineMessageIndex{
    uint64_t msgId;//错过的消息ID
    std::string groupId;//消息所属群
};

class OfflineMessageRepo{
public:
    virtual RepoResult saveOfflineMessage(const std::string& username,uint64_t msgId,const std::string& groupId)=0;//保存一条离线消息索引
    virtual std::vector<OfflineMessageIndex> listOfflineMessage(const std::string& username,size_t limit)=0;//查询用户的离线消息索引
    virtual RepoResult ackOfflineMessage(const std::string& username,const std::vector<uint64_t>& msgId)=0;//客户端确认后删除离线消息索引
};
}