#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>
#include "SqlConnection.h"
#include "config/DatabaseConfig.h"

class SqlConnectionGuard;

namespace storage{
class SqlConnectionPool{
public:
    explicit SqlConnectionPool(const DatabaseConfig& config);
    bool start();//
    void stop();
    SqlConnectionGuard acquire();//获取一个连接
    size_t size()const;
    bool healthy();
private:
    friend class SqlConnectionGuard;
    DatabaseConfig config_;
    std::vector<std::shared_ptr<SqlConnection>> connections_;//保存所有数据库连接
    std::queue<std::shared_ptr<SqlConnection>> idle_;//空闲连接队列
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool started_{false};

    void release(std::shared_ptr<SqlConnection> conn);//guard析构自动归还连接
};
}