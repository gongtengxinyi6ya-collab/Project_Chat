#pragma once
#include <memory>
#include "SqlConnection.h"
#include "SqlConnectionPool.h"

namespace storage{
class SqlConnetcionGuard{
public:
    SqlConnetcionGuard(SqlConnectionPool& pool,std::shared_ptr<SqlConnection> conn);
    ~SqlConnetcionGuard();

    SqlConnection* operator->();
    SqlConnection& operator*();
    explicit operator bool()const;
private:
    SqlConnectionPool* pool_{nullptr};
    std::shared_ptr<SqlConnection> conn_;
};
}

