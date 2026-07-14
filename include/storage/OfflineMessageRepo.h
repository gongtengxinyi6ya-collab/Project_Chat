#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <storage/RepoResult.h>
#include <storage/RepoValueResult.h>
#include "storage/types/MessageTypes.h"
/*抽象离线消息索引存储
保存消息用户名，消息ID，群ID，无需保存正文*/

namespace storage{
class OfflineMessageRepo{
public:
    virtual RepoResult saveOfflineMessage(const std::string& accountId,uint64_t msgId,const std::string& groupId)=0;//保存一条群离线消息索引
    
    virtual RepoResult saveOfflineDirectMessage(const std::string& accountId,uint64_t msgId,const std::string& peerAccountId)=0;//保存一条私聊离线消息索引
    virtual std::vector<OfflineMessageIndex> listOfflineMessage(const std::string& accountId,size_t limit)=0;//查询用户的离线消息索引
    virtual RepoResult ackOfflineMessages(const std::string& accountId,const std::vector<uint64_t>& msgId)=0;//客户端确认后删除离线消息索引
    virtual RepoValueResult<size_t> deleteCreatedBefore(int64_t cutoffMs, size_t limit) = 0;//删除离线索引
};
}