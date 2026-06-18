#include "im/MessageSyncService.h"
#include "storage/MessageRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "im/ConversationKey.h"
#include <algorithm>
im::MessageSyncService::MessageSyncService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo)
:messageRepo_(std::move(messageRepo)),offlineMessageRepo_(std::move(offlineMessageRepo)){

}

im::SyncResult im::MessageSyncService::sync(const std::string& accountId,const std::vector<SyncCursor>& cursors,size_t offlineLimit){
    if(accountId.empty()){
        return {};
    }
    SyncResult syncResult;
    if(messageRepo_){
        for(const auto& cursor:cursors){
            if(cursor.type==storage::ConversationType::Direct){//私聊
                auto conversationKey=buildDirectConversationKey(accountId,cursor.targetId);
                auto result=messageRepo_->listDirectMessagesAfter(conversationKey,cursor.lastMsgId,cursor.limit);
                nlohmann::json messagesRecordJson=nlohmann::json::array();
                uint64_t lastestMsgId=0;
                for(const auto& message:result){
                    messagesRecordJson.emplace_back(nlohmann::json{{"msgId",message.messageId},{"fromAccountId",message.senderAccountId},{"toAccountId",message.receiverAccountId},{"fromUsername",message.senderUsername},{"content",message.content},{"serverTsMs",message.serverTsMs}});
                    lastestMsgId=std::max(lastestMsgId,message.messageId);
                }
                //获取客户端本地该会话最后一条消息id
                if(result.empty()){
                    lastestMsgId=cursor.lastMsgId;
                }
                //判断是否还要更多消息
                bool hasMore=false;
                if(result.size()>=cursor.limit){
                    hasMore=true;
                }
                syncResult.deltas.emplace_back(ConversationDelta{.type=storage::ConversationType::Direct,.targetId=cursor.targetId,.fromMsgId=cursor.lastMsgId,.latestMsgId=lastestMsgId,.hasMore=hasMore,.messages=std::move(messagesRecordJson)});
            
            }
            else if(cursor.type==storage::ConversationType::Group){//群聊
                auto result=messageRepo_->listGroupMessagesAfter(cursor.targetId,cursor.lastMsgId,cursor.limit);
                uint64_t lastestMsgId=0;
                nlohmann::json messagesRecordJson=nlohmann::json::array();
                for(const auto& msg:result){
                    messagesRecordJson.emplace_back(nlohmann::json{{"msgId",msg.messageId},{"groupId",msg.groupId},{"senderAccountId",msg.senderAccountId},{"senderUsername",msg.senderUsername},{"content",msg.content},{"serverTsMs",msg.serverTsMs}});
                    lastestMsgId=std::max(lastestMsgId,msg.messageId);
                }
                 //获取客户端本地该会话最后一条消息id
                if(result.empty()){
                    lastestMsgId=cursor.lastMsgId;
                }
                //判断是否还要更多消息
                bool hasMore=false;
                if(result.size()>=cursor.limit){
                    hasMore=true;
                }
                syncResult.deltas.emplace_back(ConversationDelta{.type=storage::ConversationType::Group,.targetId=cursor.targetId,.fromMsgId=cursor.lastMsgId,.latestMsgId=lastestMsgId,.hasMore=hasMore,.messages=std::move(messagesRecordJson)});
            
        }
    }
    }
    if(offlineMessageRepo_){
        syncResult.offlineIndexes=offlineMessageRepo_->listOfflineMessage(accountId,offlineLimit);
    }
    return syncResult;
}
im::ConversationDelta im::MessageSyncService::loadDirectDelta(const std::string& selfAccountId,const std::string& peerAccountId,uint64_t lastMsgId,size_t limit){
    auto conversationKey=buildDirectConversationKey(selfAccountId,peerAccountId);
    if(conversationKey.empty()){
        return {};
    }
    if(!messageRepo_){
        return {};
    }
    auto result=messageRepo_->listDirectMessagesAfter(conversationKey,lastMsgId,limit);
    uint64_t lastestMsgId=0;
    nlohmann::json messagesRecordJson=nlohmann::json::array();
    for(const auto& message:result){
        messagesRecordJson.emplace_back(nlohmann::json{{"msgId",message.messageId},{"fromAccountId",message.senderAccountId},{"toAccountId",message.receiverAccountId},{"fromUsername",message.senderUsername},{"content",message.content},{"serverTsMs",message.serverTsMs}});
        lastestMsgId=std::max(lastestMsgId,message.messageId);
    }
     //获取客户端本地该会话最后一条消息id
    if(result.empty()){
        lastestMsgId=lastMsgId;
    }
    //判断是否还要更多消息
    bool hasMore=false;
    if(result.size()>=limit){
        hasMore=true;
    }
    return {.type=storage::ConversationType::Direct,.targetId=peerAccountId,.fromMsgId=lastMsgId,.latestMsgId=lastestMsgId,.hasMore=hasMore,.messages=std::move(messagesRecordJson)};
}
im::ConversationDelta im::MessageSyncService::loadGroupDelta(const std::string& groupId,uint64_t lastMsgId,size_t limit){
    if(groupId.empty()){
        return {};
    }
    if(!messageRepo_){
        return {};
    }
    auto result=messageRepo_->listGroupMessagesAfter(groupId,lastMsgId,limit);
    uint64_t lastestMsgId=0;
    nlohmann::json messagesRecordJson=nlohmann::json::array();

    for(const auto& msg:result){
        messagesRecordJson.emplace_back(nlohmann::json{{"msgId",msg.messageId},{"groupId",msg.groupId},{"senderAccountId",msg.senderAccountId},{"senderUsername",msg.senderUsername},{"content",msg.content},{"serverTsMs",msg.serverTsMs}});
        lastestMsgId=std::max(lastestMsgId,msg.messageId);
    }
     //获取客户端本地该会话最后一条消息id
    if(result.empty()){
        lastestMsgId=lastMsgId;
    }
    //判断是否还要更多消息
    bool hasMore=false;
    if(result.size()>=limit){
        hasMore=true;
    }
    return {.type=storage::ConversationType::Group,.targetId=groupId,.fromMsgId=lastMsgId,.latestMsgId=lastestMsgId,.hasMore=hasMore,.messages=std::move(messagesRecordJson)};
}
