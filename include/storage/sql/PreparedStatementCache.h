#pragma once
#include <memory>
#include <list>
#include <unordered_map>
#include <string>
#include <stddef.h>
#include <mysql-cppconn/jdbc>

namespace storage{

class PreparedStatementCache{
public:
    sql::PreparedStatement* getOrPrepare(sql::Connection& connection,const std::string& statementName,const std::string& sql);
    void clear();
private:
    struct Entry {
        std::unique_ptr<sql::PreparedStatement> statement;
        std::list<std::string>::iterator lruIterator;
    };

    std::size_t capacity_;
    std::list<std::string> lru_;
    std::unordered_map<std::string, Entry> entries_;
};
}