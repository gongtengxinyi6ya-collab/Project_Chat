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

    std::vector<std::string> memberAccountIds;//发送者请求被接收时的群成员快照
    std::vector<std::string> offlineAccountIds;//请求被接收时无在线连接的成员
};

struct GroupMessageWriteResult {//返回给baseLoop的多步持久化结果
    storage::RepoResult messageResult{storage::RepoStatus::Internal,"not executed"};//正文消息入库结果

    std::optional<storage::RepoResult> conversationResult;//是否更新会话列表

    std::size_t offlineAttempted{0};//计划离线所有保存数量
    std::size_t offlineSaved{0};//真正保存的离线索引数量
    std::size_t offlineFailed{0};//失败保存的离线索引数量

    std::string exceptionMessage;//异常信息
    bool durable() const noexcept{return messageResult.ok();};//消息是否真正持久化成功
    bool degraded() const noexcept{
        if(!durable()) return false;
        if(!exceptionMessage.empty()) return true;
        if(conversationResult&&!conversationResult.value().ok()) return true;
        if(offlineFailed>0) return true;
        return false;
    };//服务可用
};
}