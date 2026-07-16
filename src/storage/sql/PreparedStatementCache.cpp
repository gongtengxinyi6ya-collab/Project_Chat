#include "storage/sql/PreparedStatementCache.h"
#include <stdexcept>

namespace storage{
PreparedStatementCache::PreparedStatementCache(std::size_t capacity)
:capacity_(capacity){
    if(capacity_==0){
        throw std::invalid_argument("PreparedStatementCache capacity invalid");
    }
}

sql::PreparedStatement* PreparedStatementCache::getOrPrepare(sql::Connection& connection,std::string_view statementName,const std::string& sqlText){
    //
    std::string name(statementName);
    auto it=entries_.find(name);
    if(it!=entries_.end()){
        if(it->second.sqlText==sqlText){
            //找到且sqlText一致
            touch(it);
            return it->second.statement.get();//返回已有PreparedStatement*
        }
        else{
            //找到但Sql文本不同
            entries_.erase(name);
            throw std::logic_error("PreparedStatementCache: statementName '" +std::string(statementName) +"' reused with different SQL.");
        }

    }
    //未找到
    if(entries_.size()>=capacity_){//缓存已满
        evictOne();
    }
    auto statement=std::make_unique<sql::PreparedStatement>(connection.prepareStatement(sqlText));

    Entry entry{.sqlText=sqlText,.statement=std::move(statement),.lruIterator=lru_.begin()};
    auto [insertedIt,inserted]=entries_.emplace(std::move(name),std::move(entry));
    lru_.push_front(name);
    if (!inserted) {
        lru_.pop_front();
        throw std::logic_error(
            "failed to insert prepared statement cache entry"
        );
    }

    return insertedIt->second.statement.get();
}
void PreparedStatementCache::clear(){
    entries_.clear();
    lru_.clear();
}

void PreparedStatementCache::touch(EntryMap::iterator iterator){
    lru_.splice(lru_.begin(),lru_,iterator->second.lruIterator);
}
void PreparedStatementCache::evictOne(){
    if(lru_.empty()){
        return;
    }
    std::string name=std::move(lru_.back());
    lru_.pop_back();
    entries_.erase(name);
}
}