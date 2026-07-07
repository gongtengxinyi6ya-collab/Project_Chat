#pragma once
#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include "infra/health/HealthSnapshot.h"
#include "config/HealthConfig.h"

/*负责聚合服务健康信息，从各个基础设施模块收集状态*/
namespace infra::redis{
    class RedisClient;
}
namespace storage{
    class SqlConnectionPool;
}


namespace infra::health {

class HealthService {
public:
    HealthService();
    explicit HealthService(const HealthConfig& config);
    void setConfig(const HealthConfig& config);
    void setSqlPool(std::weak_ptr<storage::SqlConnectionPool> sqlPool);//注入SQL连接池
    void setRedisClient(std::weak_ptr<infra::redis::RedisClient> redisClient);//注入Redis客户端
    void setOnlineConnectionProvider(std::function<size_t()> provider);//注入在线连接获取函数

    HealthSnapshot snapshot();//生成完整健康快照

private:
    HealthConfig config_;
    std::chrono::steady_clock::time_point startedAt_;//健康服务创建时间

    std::weak_ptr<storage::SqlConnectionPool> sqlPool_;//指向SQL连接池，同时weak_ptr避免延长生命周期
    std::weak_ptr<infra::redis::RedisClient> redisClient_;//指向Redis客户端

    std::function<size_t()> onlineConnectionProvider_;//获取当前在线连接数

    uint64_t lastSqlAcquireTimeouts_{0};

    void checkSql(HealthSnapshot& snapshot);//读取SQL pool状态
    void checkRedis(HealthSnapshot& snapshot);//检查Redis客户端状态
    void fillRuntimeStats(HealthSnapshot& snapshot);//填充运行时信息
    void fillLoggerStats(HealthSnapshot& snapshot);
    void decideStatus(HealthSnapshot& snapshot);//根据状态计算总健康状态

    bool hasNewSqlAcquireTimeouts(const storage::SqlConnectionPoolStats& stats);
    void addReason(HealthSnapshot& snapshot, std::string reason);
};

}