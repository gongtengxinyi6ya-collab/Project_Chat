#include "im/MessageSyncService.h"
#include "storage/MessageRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "im/ConversationKey.h"
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
                for(const auto& message:result){
                    messagesRecordJson.emplace_back(nlohmann::json{{"msgId",message.messageId},{"fromAccountId",message.senderAccountId},{"toAccountId",message.receiverAccountId},{"fromUsername",message.senderUsername},{"content",message.content},{"serverTsMs",message.serverTsMs}});
                }
                syncResult.deltas.emplace_back(ConversationDelta{.type=storage::ConversationType::Direct,.targetId=cursor.targetId,.messages=std::move(messagesRecordJson)});
            
            }
            else if(cursor.type==storage::ConversationType::Group){//群聊
                auto result=messageRepo_->listGroupMessagesAfter(cursor.targetId,cursor.lastMsgId,cursor.limit);
                nlohmann::json messagesRecordJson=nlohmann::json::array();
                for(const auto& msg:result){
                    messagesRecordJson.emplace_back(nlohmann::json{{"msgId",msg.messageId},{"groupId",msg.groupId},{"senderAccountId",msg.senderAccountId},{"senderUsername",msg.senderUsername},{"content",msg.content},{"serverTsMs",msg.serverTsMs}});
                }
                syncResult.deltas.emplace_back(ConversationDelta{.type=storage::ConversationType::Group,.targetId=cursor.targetId,.messages=std::move(messagesRecordJson)});
            
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
    nlohmann::json messagesRecordJson=nlohmann::json::array();
    for(const auto& message:result){
        messagesRecordJson.emplace_back(nlohmann::json{{"msgId",message.messageId},{"fromAccountId",message.senderAccountId},{"toAccountId",message.receiverAccountId},{"fromUsername",message.senderUsername},{"content",message.content},{"serverTsMs",message.serverTsMs}});
    }
    return {.type=storage::ConversationType::Direct,.targetId=peerAccountId,.messages=std::move(messagesRecordJson)};
}
im::ConversationDelta im::MessageSyncService::loadGroupDelta(const std::string& groupId,uint64_t lastMsgId,size_t limit){
    if(groupId.empty()){
        return {};
    }
    if(!messageRepo_){
        return {};
    }
    auto result=messageRepo_->listGroupMessagesAfter(groupId,lastMsgId,limit);
    nlohmann::json messagesRecordJson=nlohmann::json::array();
    for(const auto& msg:result){
        messagesRecordJson.emplace_back(nlohmann::json{{"msgId",msg.messageId},{"groupId",msg.groupId},{"senderAccountId",msg.senderAccountId},{"senderUsername",msg.senderUsername},{"content",msg.content},{"serverTsMs",msg.serverTsMs}});
    }
    return {.type=storage::ConversationType::Group,.targetId=groupId,.messages=std::move(messagesRecordJson)};
}
