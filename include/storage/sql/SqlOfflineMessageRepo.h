#pragma once
#include <memory>
#include "storage/OfflineMessageRepo.h"

/*SQL继承OfflineMessageRepo,实现离线消息索引存储*/
namespace storage{
//向前声明
    class SqlConnectionPool;

class SqlOfflineMessageRepo:public OfflineMessageRepo{
public:
    explicit SqlOfflineMessageRepo(std::shared_ptr<SqlConnectionPool> pool);//保存连接池引用
    RepoResult saveOfflineMessage(const std::string& accountId,uint64_t msgId,const std::string& groupId)override;//保存一条离线消息索引
    
    std::vector<OfflineMessageIndex> listOfflineMessage(const std::string& accountId,size_t limit)override;//查询某用户的离线消息索引
    RepoResult ackOfflineMessages(const std::string& accountId,const std::vector<uint64_t>& msgIds)override;//客户端确认后删除离线消息索引
    RepoResult saveOfflineDirectMessage(const std::string& accountId,uint64_t msgId,const std::string& peerAccountId)override;//保存一条私聊离线消息索引
    RepoValueResult<size_t> deleteCreatedBefore(int64_t cutoffMs, size_t limit)override;//删除离线索引
private:
    std::shared_ptr<SqlConnectionPool> pool_;//从连接池获取SQL连接
};
}