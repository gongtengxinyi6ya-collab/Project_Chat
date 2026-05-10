#pragma once
#include <string>
#include "SqlResult.h"
#include "config/DatabaseConfig.h"
/*
管理数据库连接
封装MySql/C++,API
统一错误处理*/
namespace storage{
class SqlConnection{
public:
    explicit SqlConnection(const DatabaseConfig& config);
    ~SqlConnection();
    bool connect();//建立数据库连接
    void close();//关闭连接
    bool ping();//健康检查
    SqlResult execute(const std::string& sql);//执行数据库操插入、更新，删除，DDL操作
    SqlResult query(const std::string& sql);//执行查询操作
    bool connected()const;//判断连接状态
private:
    DatabaseConfig config_;
    bool connected_{false};
};
}