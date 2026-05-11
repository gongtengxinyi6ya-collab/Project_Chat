#pragma once
#include <memory>
#include "SqlConnection.h"

class SqlConnectionPool;
/*RALL管理连接，离开作用域自动归还连接池*/
namespace storage{
class SqlConnectionGuard{
public:
    SqlConnectionGuard(SqlConnectionPool& pool,std::shared_ptr<SqlConnection> conn);
    ~SqlConnectionGuard();//调用release归还连接

    SqlConnection* operator->();//支持guard->execute()直接调用连接方法
    SqlConnection& operator*();//支持*guard获取连接对象
    explicit operator bool()const;//必须显示转换，允许if(guard)判断连接是否有效
private:
    SqlConnectionPool* pool_{nullptr};//所属连接池
    std::shared_ptr<SqlConnection> conn_;//当前持有连接
};
}

