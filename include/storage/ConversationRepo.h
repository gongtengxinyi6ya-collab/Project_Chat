#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/RepoResult.h"
/*会话列表，作为最近聊天入口，显示最后一条消息+未读数+更新时间*/
namespace storage{
enum class ConversationType:uint8_t{//区分会话类型
    Direct=1,
    Group=2
};
struct ConversationSummary{
    std::string ownerAccountId{};//会话属于哪个账号
    ConversationType type{ConversationType::Direct};//会话类型
    std::string targetId{};//对方id，accountId/groupId
    uint64_t lastMsgId{0};//会话最后一条消息ID
    std::string lastPreview{};//最后一条消息预览
    std::string lastSenderAccountId{};//最后一条消息发送者
    std::string lastSenderUsername{};//最后一条消息发送者用户名或昵称
    uint64_t lastTsMs{0};//最后一条消息时间
    uint32_t unreadCount{0};//当前未读数
    uint64_t lastReadMsgId{0};//当前账号已读到的最大消息ID
    uint64_t lastReadAtMs{0};//最近一次已读时间
};

class ConversationRepo{
public:
    virtual ~ConversationRepo()=default;
    virtual RepoResult upsertDirectOnMessage(const std::string&senderAccountId,const std::string&receiverAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs)=0;//私聊消息发送成功落库后，更新发送方和接收方的会话行
    virtual std::vector<ConversationSummary> listConversations(const std::string& ownerAccountId,size_t limit)=0;//查询某个账号会话列表
    virtual RepoResult markConversationRead(const std::string&ownerAccountId,ConversationType type,const std::string& targetId,uint64_t readMsgId,uint64_t readAtMs)=0;//清空某个会话未读数，并记录读到哪个消息
    virtual RepoResult upsertGroupOnMessage(const std::string&groupId,const std::vector<std::string>&memberAccountIds,const std::string& senderAccountId,const std::string&senderUsername,uint64_t msgId,const std::string& preview,uint64_t serverTsMs)=0;//群消息保存成功后，为所有群成员更新会话列表
};

inline std::string conversationTypeToString(ConversationType type){
    switch(type){
        case ConversationType::Group:
            return "group";
        case ConversationType::Direct:
            return "direct";
        default:
            return "group";
    }
}
}