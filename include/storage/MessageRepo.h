#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"

/*后续保存群消息，私聊消息，离线消息*/
namespace storage{
class SaveMessageResult{
public:
    RepoStatus status{RepoStatus::Ok};
    uint64_t messageId{0};
    std::string message{};

    bool ok()const{return status==RepoStatus::Ok;}
};
class MessageRepo{
public:
struct MessageRecord{
    uint64_t messageId;
    std::string groupId;
    std::string senderAccountId;
    std::string senderUsername;
    std::string content;
    uint64_t serverTsMs;
};
    virtual ~MessageRepo()=default;
    virtual SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string&senderAccountId,const std::string& senderUsername,const std::string&content,uint64_t serverTsMs)=0;//保存群消息
    virtual std::vector<MessageRecord> listGroupMessages(const std::string&groupId,uint64_t beforeMsgId,size_t limit)=0;//查询群历史消息
};
}