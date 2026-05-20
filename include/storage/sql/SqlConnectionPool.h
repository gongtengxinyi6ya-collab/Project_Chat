#pragma once
#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>

#include "config/DatabaseConfig.h"
#include "SqlConnectionGuard.h"
/*维护固定数量SQL连接*/
namespace storage{
    class SqlConnection;

class SqlConnectionPool{
public:
    explicit SqlConnectionPool(const DatabaseConfig& config);
    bool start();//创建连接
    void stop();//关闭连接并清空连接
    SqlConnectionGuard acquire();//获取一个连接
    size_t size()const;
    bool healthy();
private:
    friend class SqlConnectionGuard;
    DatabaseConfig config_;//数据库配置
    std::vector<std::shared_ptr<SqlConnection>> connections_;//保存所有数据库连接
    std::queue<std::shared_ptr<SqlConnection>> idle_;//空闲连接队列
    mutable std::mutex mutex_;
    std::condition_variable cv_;//等待空闲连接
    bool started_{false};//连接池是否启动

    void release(std::shared_ptr<SqlConnection> conn);//guard析构自动归还连接
};
}