#pragma once
#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <atomic>
#include "config/DatabaseConfig.h"
#include "SqlConnectionGuard.h"
#include "storage/sql/SqlPoolStats.h"
#include "storage/sql/SqlConnectionPoolOptions.h"
/*维护固定数量SQL连接*/
namespace storage{
    class SqlConnection;

class SqlConnectionPool{
public:

    explicit SqlConnectionPool(const DatabaseConfig& databaseConfig,SqlConnectionPoolOptions options);

    const std::string& name() const noexcept {
        return options_.name;
    }

    bool start();//创建连接
    void stop();//关闭连接并清空连接
    SqlConnectionGuard acquire();//获取一个连接
    size_t size()const;
    bool healthy();

    SqlConnectionGuard acquireFor(std::chrono::milliseconds timeout);//有超时时间地获取连接
    SqlConnectionPoolStats stats() const;
    bool replaceConnection(const std::shared_ptr<SqlConnection>& oldConn);
private:
    friend class SqlConnectionGuard;
    DatabaseConfig config_;//数据库配置
    SqlConnectionPoolOptions options_;//连接池选择
    std::vector<std::shared_ptr<SqlConnection>> connections_;//保存所有数据库连接
    std::queue<std::shared_ptr<SqlConnection>> idle_;//空闲连接队列
    mutable std::mutex mutex_;
    std::condition_variable cv_;//等待空闲连接

    std::chrono::milliseconds acquireTimeout_{3000};

    std::atomic<uint64_t> reconnects_{0};//成功替换坏连接次数
    std::atomic<uint64_t> replaceFailures_{0};//坏连接替换失败次数
    std::atomic<uint64_t> acquireCount_{0};//成功获取连接次数
    std::atomic<uint64_t> acquireTimeouts_{0};//

    std::atomic<bool> started_{false};//连接池是否启动

    void release(std::shared_ptr<SqlConnection> conn);//guard析构自动归还连接
};
}