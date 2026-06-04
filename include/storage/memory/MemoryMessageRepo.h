#pragma once
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>
#include <algorithm>
#include <atomic>
#include <unordered_map>
#include "storage/MessageRepo.h"
/*内存储存实现，后续可替换为数据库实现*/
namespace storage{
class MemoryMessageRepo:public storage::MessageRepo{
public:
    storage::SaveMessageResult saveGroupMessage(uint64_t,const std::string& groupId,const std::string& senderAccountId,const std::string& ,const std::string&content,uint64_t serverTsMs)override;//保存群消息
    std::vector<MessageRecord> listGroupMessages(const std::string&groupId,uint64_t beforeMsgId,size_t limit)override;//查询群历史消息，最多返回limit条
    SaveMessageResult saveDirectMessage(uint64_t msgId,const std::string&conversationKey,const std::string&senderAccountId,const std::string& receiverAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs) override;//保存私聊消息
    std::vector<DirectMessageRecord> listDirectMessages(const std::string& conversationKey,uint64_t beforeMsgId,size_t limit) override;//查询某个私聊会话的历史消息
private:
    std::unordered_map<std::string,std::vector<MessageRecord>> groupMessages_;//groupId映射消息列表

    mutable std::mutex mutex_;//保护groupMessages_的读写
};
}
