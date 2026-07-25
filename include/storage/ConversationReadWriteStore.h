#pragma once
#include "storage/RepoValueResult.h"
#include "storage/types/ConversationTypes.h"

/*
负责将一次会话已读操作作为完整事务执行*/
namespace storage{
class ConversationReadWriteStore{
public:
    virtual RepoValueResult<ConversationReadResult> commit(const ConversationReadCommand& command)=0;//校验命令，工具会话类型加入私聊或群聊事务
    virtual ~ConversationReadWriteStore()=default;
};
}