#include "im/MessageAckService.h"
#include "storage/MessageRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "storage/ConversationRepo.h"
#include <stdexcept>
im::MessageAckService::MessageAckService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo,std::shared_ptr<storage::ConversationRepo> conversationRepo)
:messageRepo_(std::move(messageRepo)),offlineMessageRepo_(std::move(offlineMessageRepo)),conversationRepo_(std::move(conversationRepo)){
    if(!messageRepo_||!offlineMessageRepo_||!conversationRepo_){
        throw std::invalid_argument("MessageAckService: null dependency");
    }
}
storage::RepoValueResult<storage::MessageAckResult> im::MessageAckService::ackMessages(const std::string& accountId,const std::vector<uint64_t>& msgIds,int64_t ackAtMs){
    if(accountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(msgIds.empty()){
        //幂等OK
        return {.status=storage::RepoStatus::Ok};
    }
    if(!messageRepo_){
        return {.status=storage::RepoStatus::Internal,.message="messageRepo is not avaiable"};
    }
    auto result=messageRepo_->markDelivered(accountId,msgIds,ackAtMs);
    if(!result.ok()){
        return {.status=result.status,.message=result.message};
    }
    if(!result.value.has_value()){
        return {.status=storage::RepoStatus::Internal,.message="value from markDelivered valiad"};
    }
    auto ackedCount=result.value.value();
    return {.status=storage::RepoStatus::Ok,.value=storage::MessageAckResult{.requestedCount=msgIds.size(),.ackedCount=ackedCount,.ignoredCount=msgIds.size()-ackedCount}};

}
storage::RepoResult im::MessageAckService::ackOfflineMessages(const std::string&accountId,const std::vector<uint64_t>& offlineMsgIds){
    if(accountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(offlineMsgIds.empty()){
        //幂等OK
        return {.status=storage::RepoStatus::Ok};
    }
    if(!offlineMessageRepo_){
        return {.status=storage::RepoStatus::Internal,.message="offlineMessageRepo is not avaiable"};
    }
    return offlineMessageRepo_->ackOfflineMessages(accountId,offlineMsgIds);
}
storage::RepoValueResult<storage::ConversationReadResult> im::MessageAckService::markConversationRead(const std::string&accountId,storage::ConversationType type,const std::string&targetId,uint64_t readMsgId,int64_t readAtMs){
    if(accountId.empty()||targetId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument,.message="accountId or targetId is empty"};
    }
    if(type==storage::ConversationType::Unknown||readMsgId==0){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!conversationRepo_||!messageRepo_){
        return {.status=storage::RepoStatus::Internal,.message="conversationRepo or messageRepo is not avaiable"};
    }
    auto resultConversation=conversationRepo_->markConversationRead(accountId,type,targetId,readMsgId,readAtMs);
    if(!resultConversation.ok()){
        return {.status=resultConversation.status,.message=resultConversation.message};
    }
    auto resultMessage=messageRepo_->markReadBefore(accountId,type,targetId,readMsgId,readAtMs);
    if(!resultMessage.ok()){
        return {.status=resultMessage.status,.message=resultMessage.message};
    }
    if(!resultMessage.value.has_value()){
        return {.status=storage::RepoStatus::Internal,.message="markReadBefore value invalid"};
    }
    return {.status=storage::RepoStatus::Ok,.value=storage::ConversationReadResult{.type=type,.targetId=targetId,.readMsgId=readMsgId,.readAtMs=readAtMs,.receiptUpdated=resultMessage.value.value()}};
}