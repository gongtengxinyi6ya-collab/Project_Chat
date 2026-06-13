#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"
#include "storage/types/ConversationTypes.h"
/*会话列表，作为最近聊天入口，显示最后一条消息+未读数+更新时间*/
namespace storage{

class ConversationRepo{
public:
    virtual ~ConversationRepo()=default;
    virtual RepoResult upsertDirectOnMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs)=0;//私聊消息发送成功落库后，更新发送方和接收方的会话行
    virtual std::vector<ConversationSummary> listConversations(const std::string& ownerAccountId,size_t limit)=0;//查询某个账号会话列表
    virtual RepoResult markConversationRead(const std::string&ownerAccountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,uint64_t readAtMs)=0;//清空某个会话未读数，并记录读到哪个消息
    virtual RepoResult upsertGroupOnMessage(const std::string&groupId,const std::vector<std::string>&memberAccountIds,const std::string& senderAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs)=0;//群消息保存成功后，为所有群成员更新会话列表
};

}