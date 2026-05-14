#pragma once
#include <string>
#include <cstdint>
#include "storage/RepoResult.h"

/*后续保存群消息，私聊消息，离线消息*/
namespace storage{
class SaveMessageResult{
public:
    RepoStatus status{RepoStatus::Ok};
    uint64_t messageId{0};
    std::string message;

    bool ok()const{return status==RepoStatus::Ok;}
};
class MessageRepo{
public:
    virtual ~MessageRepo()=default;
    virtual SaveMessageResult saveGroupMessage(uint64_t msgId,const std::string& groupId,const std::string&from,const std::string&content,uint64_t serverTsMs)=0;//保存群消息
    
};
}