#pragma once

#include <cstdint>
#include <string>

#include "storage/RepoResult.h"

namespace im {
//私聊持久化快照
struct DirectMessageWriteCommand {
    std::uint64_t msgId{0};//消息id
    std::uint64_t serverTsMs{0};//服务端接收消息时间

    std::string conversationKey;//双方账号会话键
    std::string senderAccountId;//发送者1账号
    std::string receiverAccountId;//接收者账号
    std::string senderUsername;//用户名
    std::string content;//消息内容
};

struct DirectMessageWriteResult {//工作线程返回给baseLoop结果
    storage::RepoResult commitResult{storage::RepoStatus::Internal,"not executed"};//事务整体结果

    std::int64_t queueWaitUs{0};//线程池等待时间
    std::int64_t persistUs{0};//MySQL事务耗时
    std::string exceptionMessage;//异常文本

    bool durable() const noexcept {//整个事务提交成功
        return commitResult.ok();
    }
};

}