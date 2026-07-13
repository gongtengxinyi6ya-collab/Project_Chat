#include "im/GroupMessagePersistenceService.h"
#include "storage/MessageRepo.h"
#include "storage/ConversationRepo.h"
#include "storage/OfflineMessageRepo.h"
#include <stdexcept>
#include <exception>
namespace im{

GroupMessagePersistenceService::GroupMessagePersistenceService(std::shared_ptr<storage::MessageRepo> messageRepo,std::shared_ptr<storage::ConversationRepo> conversationRepo,std::shared_ptr<storage::OfflineMessageRepo> offlineMessageRepo)
:messageRepo_(std::move(messageRepo)),conversationRepo_(std::move(conversationRepo)),offlineMessageRepo_(std::move(offlineMessageRepo)){
    if(!messageRepo_){
        throw std::invalid_argument("messageRepo invalid");
    }
}
GroupMessageWriteResult GroupMessagePersistenceService::persist(const GroupMessageWriteCommand& command) const{
    if(command.groupId.empty()||command.msgId<0||command.senderAccountId.empty()){
        return GroupMessageWriteResult{.messageResult={storage::RepoStatus::InvalidArgument}};
    }
    GroupMessageWriteResult groupMsgWriteRes;
    try{
        auto result=messageRepo_->saveGroupMessage(command.msgId,command.groupId,command.senderAccountId,command.senderUsername,command.content,command.serverTsMs);
        groupMsgWriteRes.messageResult={.status=result.status,.message=result.message};
        if(!result.ok()){//消息保存失败返回结果
            return groupMsgWriteRes;
        }
        //消息保存成功更新会话
        auto resultConver=conversationRepo_->upsertGroupOnMessage(command.groupId,command.memberAccountIds,command.senderAccountId,command.senderUsername,command.msgId,command.content,command.serverTsMs);
        if(!resultConver.ok()){
            //会话更新失败
            groupMsgWriteRes.conversationResult=resultConver;
        }
        //保存离线索引
        for(const auto& account:command.offlineAccountIds){
            auto resultOffline=offlineMessageRepo_->saveOfflineMessage(account,command.msgId,command.groupId);
            groupMsgWriteRes.offlineAttempted++;
            if(!resultOffline.ok()){
                groupMsgWriteRes.offlineFailed++;
            }
            else{
                groupMsgWriteRes.offlineSaved++;
            }
        }
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