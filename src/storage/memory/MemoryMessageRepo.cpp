#include "storage/memory/MemoryMessageRepo.h"

storage::SaveMessageResult storage::MemoryMessageRepo::saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string& from,const std::string& content,uint64_t serverTsMs){
    if(groupId.empty()||from.empty()||content.empty()){
        return {RepoStatus::InvalidArgument,0,""};
    }
    MessageRecord record;
    record.messageId=msgId;
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
std::vector<storage::MessageRepo::MessageRecord> storage::MemoryMessageRepo::listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit){
    auto it=groupMessages_.find(groupId);
    if(it==groupMessages_.end()){
        return {};
    }
    std::vector<MessageRecord> res;
    const auto& messages=it->second;
    if(beforeMsgId==0){
        if(messages.size()>limit){
            res.insert(res.end(),messages.end()-limit,messages.end());
        }
        else{
            return messages;
        }
    }
    else{
        auto pos=std::lower_bound(messages.begin(),messages.end(),beforeMsgId,[](const MessageRecord& msg,uint64_t id){
            return msg.messageId<id;
        });
        if(std::distance(messages.begin(),pos)>limit){
            res.insert(res.end(),pos-limit,pos);
        }
        else{
            res.insert(res.end(),messages.begin(),pos);
        }
    }
    return res;
}