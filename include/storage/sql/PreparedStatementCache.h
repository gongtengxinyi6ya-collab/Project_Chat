#pragma once
#include <memory>
#include <list>
#include <unordered_map>
#include <string>
#include <stddef.h>
#include <mysql/jdbc.h>

namespace storage{

/*管理单个MySQL连接上的PreparedStatement*/
class PreparedStatementCache{
public:
    sql::PreparedStatement* getOrPrepare(sql::Connection& connection,const std::string& statementName,const std::string& sql);
    void clear();
private:
    struct Entry {
        std::string sqlText;//创建语句时的SQL
        std::unique_ptr<sql::PreparedStatement> statement;//原生预编译对象
        std::list<std::string>::iterator lruIterator;//对应LRU节点
    };

    std::size_t capacity_{0};//最大语句数量
    std::list<std::string> lru_;//保存语句key的最近使用顺序
    std::unordered_map<std::string, Entry> entries_;//根据语句key查找PreparedStatement
};
}