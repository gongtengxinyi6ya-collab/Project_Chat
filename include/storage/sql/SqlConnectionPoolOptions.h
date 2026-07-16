#pragma once
#include <string>
#include <cstddef>
#include <chrono>
#include <stdexcept>
/*分离数据库账号地址和连接池运行参数*/
namespace{
struct SqlConnectionPoolOptions{
    std::string name;//连接池名称
    std::size_t poolSize{0};//连接数量
    std::chrono::milliseconds acquireTimeout{0};//获取连接等待时间
    std::size_t statementCacheSize{0};//连接缓存语句数量

    void validate()const{
        if (name.empty()) {
            throw std::invalid_argument("SQL pool name is empty");
        }
        if (poolSize == 0) {
            throw std::invalid_argument("SQL pool size is zero");
        }
        if (acquireTimeout.count() <= 0) {
            throw std::invalid_argument(
                "SQL pool acquire timeout is invalid");
        }
        if (statementCacheSize == 0) {
            throw std::invalid_argument(
                "statement cache size is zero");
        }
    }
};

}