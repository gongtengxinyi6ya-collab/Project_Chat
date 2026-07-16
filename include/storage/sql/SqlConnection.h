#pragma once
#include <string>
#include <mysql/jdbc.h>
#include <memory>
#include <iostream>
#include <vector>
#include "SqlResult.h"
#include "config/DatabaseConfig.h"
#include "SqlParam.h"
/*
管理数据库连接
封装MySql/C++,API
统一错误处理*/
namespace storage{
class PreparedStatementCache;
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

    SqlResult executePrepared(const std::string& sql,const std::vector<SqlParam>& params);
    SqlResult queryPrepared(const std::string& sql,const std::vector<SqlParam>& params);

    uint64_t fetchLastInsertId();//获取最近插入Id
    SqlResult executePreParedInsert(const std::string& sql,const std::vector<SqlParam>& params);

    SqlConnection(const DatabaseConfig& config,std::size_t statementCacheSize);
    //PreparedStatementCache缓存接口
    SqlResult executePrepared(std::string_view statementName,const std::string& sql,const std::vector<SqlParam>& params);
    SqlResult queryPrepared(std::string_view statementName,const std::string& sql,const std::vector<SqlParam>& params);
    //事务接口
    SqlResult beginTransaction();//关闭自动提交，进入事务
    SqlResult commit();//提交事务并恢复自动提交
    SqlResult rollback();//回滚事务并恢复自动提交
    bool inTransaction()const;//标记当前连接是否处于事务状态
    void resetSessionState();//连接归还连接池前，确保不在事务中

    bool ensureConnected();//执行SQL前确认连接可用
    bool reconnect();//重新建立MySQL连接
    bool resetSessionStateSafe();//连接归还连接池前安全恢复状态
    void markBroken();//标记连接不可继续复用
    bool broken() const;//连接池判断是否要把连接重新放回idle队列

    uint64_t reconnectCount() const{return reconnectCount_;}
    bool isConnectionError(const sql::SQLException& e) const;
private:
    DatabaseConfig config_;
    bool connected_{false};
    std::unique_ptr<sql::Connection> conn_;//MySql连接对象
    sql::Driver* driver_{nullptr};//驱动入口
    std::unique_ptr<PreparedStatementCache> statementCache_;

    SqlResult readResultSet(sql::ResultSet* resultSet);

    bool inTransaction_{false};//标记当前事务是否处于事务状态
    bool autoCommit_{true};
    bool broken_{false};
    uint64_t reconnectCount_{0};
};
}