#include "im/GroupMessagePersistenceService.h"
#include "storage/sql/SqlGroupMessageWriteStore.h"
#include <stdexcept>
#include <exception>
#include <chrono>
namespace im{

GroupMessagePersistenceService::GroupMessagePersistenceService(std::shared_ptr<storage::GroupMessageWriteStore> writeStore)
:writeStore_(std::move(writeStore)){
    if(!writeStore_){
        throw std::invalid_argument("writeStore invalid");
    }
}
GroupMessageWriteResult GroupMessagePersistenceService::persist(const GroupMessageWriteCommand& command) const{
    if(command.groupId.empty()||command.msgId==0||command.senderAccountId.empty()){
        return GroupMessageWriteResult{.commitResult={storage::RepoStatus::InvalidArgument}};
    }
    if(writeStore_){
        return GroupMessageWriteResult{.commitResult={storage::RepoStatus::Internal}};
    }
    GroupMessageWriteResult groupMsgWriteRes;
    try{
        //计算SQL开始时间
        auto start=std::chrono::steady_clock::now();
        auto result=writeStore_->commit(command);
        groupMsgWriteRes.commitResult={.status=result.status,.message=result.message};
        if(result.ok()&&result.value.has_value()){
            //成功保存groupSeq
            groupMsgWriteRes.groupSeq=result.value.value();
        }
        //计算持久化时间
        auto end=std::chrono::steady_clock::now();
        groupMsgWriteRes.persistUs=std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

        return groupMsgWriteRes;
    }catch(const std::exception& e){
        groupMsgWriteRes.exceptionMessage=e.what();
        return groupMsgWriteRes;
    }catch(...){
        groupMsgWriteRes.exceptionMessage="unknow exception";
        return groupMsgWriteRes;
    }
}
}