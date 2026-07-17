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
    if(statementName.empty()){
        throw std::invalid_argument("statementName is empty");
    }
    std::string name(statementName);
    auto it=entries_.find(name);
    if(it!=entries_.end()){
        if(it->second.sqlText!=sqlText){
            lru_.erase(it->second.lruIterator);
            entries_.erase(it);
        }
        else{
            //找到且sqlText一致
            touch(it);
            return it->second.statement.get();//返回已有PreparedStatement*
        }
    }
    //未找到
    if(entries_.size()>=capacity_){//缓存已满
        evictOne();
    }
    std::unique_ptr<sql::PreparedStatement> statement(connection.prepareStatement(sqlText));
    if(!statement){
        throw std::runtime_error("prepareStatement returned nullptr");
    }
    lru_.push_front(name);
    try{
        Entry entry{.sqlText=sqlText,
            .statement=std::move(statement),
            .lruIterator=lru_.begin()};
        auto [insertedIt,inserted]=entries_.emplace(std::move(name),std::move(entry));
        
        if (!inserted) {
            lru_.pop_front();
            throw std::logic_error(
                "failed to insert prepared statement cache entry"
            );
        }
        return insertedIt->second.statement.get();
    }catch(...){
        if (!lru_.empty() && lru_.front() == name) {
            lru_.pop_front();
        }
        throw;
    }
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