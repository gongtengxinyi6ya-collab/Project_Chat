#include "im/DirectMessagePersistenceService.h"
#include <chrono>
#include "storage/DirectMessageWriteStore.h"
namespace im{
DirectMessagePersistenceService::DirectMessagePersistenceService(std::shared_ptr<storage::DirectMessageWriteStore> writeStore)
:writeStore_(std::move(writeStore)){
    if (!writeStore_) {
    throw std::invalid_argument("DirectMessagePersistenceService: null write store");
}
}

DirectMessageWriteResult DirectMessagePersistenceService::persist(const DirectMessageWriteCommand& command) const{
    if(command.conversationKey.empty()||command.msgId==0||command.senderAccountId.empty()||command.receiverAccountId.empty()){
        return {.commitResult={storage::RepoStatus::InvalidArgument}};
    }
    if(!writeStore_){
        return {.commitResult={storage::RepoStatus::Internal}};
    }
    DirectMessageWriteResult msgWriteRes;
    auto start=std::chrono::steady_clock::now();
    try{
        //计算SQL开始时间
        auto result=writeStore_->commit(command);
        msgWriteRes.commitResult={.status=result.status,.message=result.message};

        return msgWriteRes;
    }catch(const std::exception& e){

        msgWriteRes.exceptionMessage=e.what();
        msgWriteRes.commitResult = {storage::RepoStatus::SqlError,e.what()
};
        return msgWriteRes;
    }catch(...){
        msgWriteRes.exceptionMessage="unknow exception";
        msgWriteRes.commitResult = { storage::RepoStatus::Internal,"unknown direct message persistence exception"
};
        //计算持久化时间
        auto end=std::chrono::steady_clock::now();
        msgWriteRes.persistUs=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
        return msgWriteRes;
    }
}
}