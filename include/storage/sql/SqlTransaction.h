#pragma once
/*作为RALL事务对象：
构造时开启事务；commit（）成功后提交；若对象析构时还没提交，自动rollback;*/

namespace storage{
    class SqlConnection;

class SqlTransaction{
public:
    explicit SqlTransaction(SqlConnection& conn);//绑定连接并开启事务
    ~SqlTransaction();//自动兜底回滚
    void commit();//提交事务
    void rollback();//主动回滚事务

    //禁止拷贝，防止事务对象被复制后重复commit和rollback
    SqlTransaction(const SqlTransaction&)=delete;
    SqlTransaction& operator=(const SqlTransaction&)=delete;

private:
    SqlConnection& conn_;//当前事务绑定的数据库连接
    bool active_{false};//标记事务是否处于未提交/未回滚状态
};
}