#include "storage/memory/MemoryMessageRepo.h"

storage::SaveMessageResult storage::MemoryMessageRepo::saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string& senderAccountId,const std::string& senderUsername,const std::string& content,uint64_t serverTsMs){
    if(groupId.empty()||senderAccountId.empty()||content.empty()){
        return {RepoStatus::InvalidArgument,0,""};
    }
    MessageRecord record;
    record.messageId=msgId;
    record.groupId=groupId;
    record.senderAccountId=senderAccountId;
    record.senderUsername=senderUsername;
    record.content=content;
    record.serverTsMs=serverTsMs;
    {
        std::lock_guard lk(mutex_);
        groupMessages_[groupId].push_back(std::move(record));
    }
    return {RepoStatus::Ok,record.messageId,""};
}
std::vector<storage::MessageRepo::MessageRecord> storage::MemoryMessageRepo::listGroupMessages(const std::string& groupId,uint64_t beforeMsgId,size_t limit){
    if(groupId.empty()){
        return {};
    }
    std::lock_guard lk(mutex_);
    auto it=groupMessages_.find(groupId);
    if(it==groupMessages_.end()){
        return {};
    }
    std::vector<MessageRecord> res;
    const auto& messages=it->second;
    if(beforeMsgId==0){//当beforeMsgId为0时，返回最新的limit条消息
        if(messages.size()>limit){//如果消息数量超过limit，返回最新的limit条
            res.insert(res.end(),std::prev(messages.end(),limit),messages.end());
        }
        else{//否则返回所有消息
            return messages;
        }
    }
    else{//当beforeMsgId不为0时，返回messageId小于beforeMsgId的消息，最多返回limit条
        auto pos=std::lower_bound(messages.begin(),messages.end(),beforeMsgId,[](const MessageRecord& msg,uint64_t id){
            return msg.messageId<id;
        });
        auto index=static_cast<size_t>(std::distance(messages.begin(),pos));
        if(index>limit){
            res.insert(res.end(),std::prev(pos,limit),pos);//迭代器从pos向前移动limit位置
        }
        else{
            res.insert(res.end(),messages.begin(),pos);
        }
    }
    return res;
}