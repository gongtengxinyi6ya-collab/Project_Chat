#include "storage/memory/MemoryMessageRepo.h"

storage::SaveMessageResult storage::MemoryMessageRepo::saveGroupMessage(const std::string& groupId,const std::string& from,const std::string& content,uint64_t serverTsMs){
    if(groupId.empty()||from.empty()||content.empty()){
        return {RepoStatus::InvalidArgument,0,""};
    }
    MessageRecord record;
    record.messageId=nextMessageId_++;
    record.groupId=groupId;
    record.from=from;
    record.content=content;
    record.serverTsMs=serverTsMs;
    {
        std::lock_guard lk(mutex_);
        groupMessages_[groupId].push_back(std::move(record));
    }
    return {RepoStatus::Ok,record.messageId,""};
}