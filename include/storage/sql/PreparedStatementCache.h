#pragma once
#include <memory>
#include <list>
#include <unordered_map>
#include <string>
#include <stddef.h>
#include <mysql/jdbc.h>

namespace storage{

/*管理单个MySQL连接上的PreparedStatement缓存，基于LRU淘汰策略
*/
class PreparedStatementCache{
public:
    explicit PreparedStatementCache(std::size_t capacity);

    sql::PreparedStatement* getOrPrepare(sql::Connection& connection,std::string_view statementName,const std::string& sql);
    void clear();
    std::size_t size() const noexcept {
        return entries_.size();
    }
    std::size_t capacity() const noexcept {
        return capacity_;
    }
private:
    struct Entry {
        std::string sqlText;//创建语句时的SQL
        std::unique_ptr<sql::PreparedStatement> statement;//原生预编译对象
        std::list<std::string>::iterator lruIterator;//对应LRU节点位置
    };
    using EntryMap=std::unordered_map<std::string,Entry>;//根据名称快速查找缓存项
    std::size_t capacity_{0};//最大语句数量
    std::list<std::string> lru_;//保存语句key的最近使用顺序
    EntryMap entries_;//根据语句key查找PreparedStatement
    void touch(EntryMap::iterator iterator);//缓存命中,将其移动到LRU队头
    void evictOne();//缓存满时删除最后一个名称
};
}