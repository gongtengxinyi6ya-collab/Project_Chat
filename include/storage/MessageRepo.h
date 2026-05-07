#pragma once
#include <string>
#include <cstdint>

/*后续保存群消息，私聊消息，离线消息*/
class MessageRepo{
public:
    virtual ~MessageRepo()=default;
    virtual uint64_t saveGroupMessage(const std::string& groupId,const std::string&from,const std::string&content,uint64_t serverTsMs)=0;//保存群消息
    
};
