#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include "storage/RepoResult.h"

namespace im{
struct GroupMessageWriteCommand {//提交给工作线程的不可变群消息持久化快照
    std::uint64_t msgId{0};//消息全局id
    std::uint64_t serverTsMs{0};//服务端接收消息时间

    std::string groupId;//目标群id
    std::string senderAccountId;//发送者唯一账号id
    std::string senderUsername;//发送消息用户名
    std::string content;//消息正文

    
};

struct GroupMessageWriteResult {//返回给baseLoop的多步持久化结果
    storage::RepoResult commitResult{storage::RepoStatus::Internal,"not executed"};//正文消息入库结果

    std::uint64_t groupSeq;//数据库分配的群内序号

    std::string exceptionMessage{};//异常信息
    std::int64_t queueWaitUs{0};//线程池队列等待的时间
    std::int64_t persistUs{0};//SQL事务耗时

    bool durable() const noexcept{return commitResult.ok()&&groupSeq>0;};//消息是否真正持久化成功
    
};
}