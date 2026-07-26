#include "im/MessageAckService.h"
#include "storage/MessageRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "storage/ConversationReadWriteStore.h"
#include <stdexcept>
im::MessageAckService::MessageAckService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo,std::shared_ptr<storage::ConversationReadWriteStore> conversationStore)
:messageRepo_(std::move(messageRepo)),offlineMessageRepo_(std::move(offlineMessageRepo)),conversationReadWriteStore_(std::move(conversationStore)){
    if(!messageRepo_||!offlineMessageRepo_||!conversationReadWriteStore_){
        throw std::invalid_argument("MessageAckService: null dependency");
    }
}
storage::RepoValueResult<storage::MessageAckResult> im::MessageAckService::ackMessages(const std::string& accountId,const std::vector<uint64_t>& msgIds,int64_t ackAtMs){
    if(accountId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(msgIds.empty()){
        //幂等OK
        return {.status=storage::RepoStatus::Ok,.value=storage::MessageAckResult{.requestedCount=0,.ackedCount=0,.ignoredCount=0}};
    }
    if(!messageRepo_){
        return {.status=storage::RepoStatus::Internal,.message="messageRepo is not avaiable"};
    }
    auto result=messageRepo_->markDeliveredBatch(accountId,msgIds,ackAtMs);
    if(!result.ok()){
        return {.status=result.status,.message=result.message};
    }
    if(!result.value.has_value()){
        return {.status=storage::RepoStatus::Internal,.message="value from markDelivered valiad"};
    }

    return {.status=storage::RepoStatus::Ok,.value=result.value.value()};

}
storage::RepoValueResult<size_t> im::MessageAckService::ackOfflineMessages(const std::string&accountId,const std::vector<uint64_t>& offlineMsgIds){
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
    return offlineMessageRepo_->ackOfflineMessagesBatch(accountId,offlineMsgIds);
}
storage::RepoValueResult<storage::ConversationReadResult> im::MessageAckService::markConversationRead(const std::string&accountId,storage::ConversationType type,const std::string&targetId,uint64_t readMsgId,int64_t readAtMs){
    if(accountId.empty()||targetId.empty()){
        return {.status=storage::RepoStatus::InvalidArgument,.message="accountId or targetId is empty"};
    }
    if(type==storage::ConversationType::Unknown||readMsgId==0){
        return {.status=storage::RepoStatus::InvalidArgument};
    }
    if(!conversationReadWriteStore_){
        return {.status=storage::RepoStatus::Internal,.message="conversationReadWriteStore is not avaiable"};
    }
    return conversationReadWriteStore_->commit(storage::ConversationReadCommand{
        .accountId=accountId,
        .type=type,
        .targetId=targetId,
        .readMsgId=readMsgId,
        .readAtMs=readAtMs
    });
}