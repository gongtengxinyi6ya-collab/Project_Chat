#include "storage/sql/SqlParam.h"
#include <mysql/jdbc.h>
storage::SqlParam::SqlParam(std::string v):value_(v){}
storage::SqlParam::SqlParam(const char* v):value_(v){}
storage::SqlParam::SqlParam(int64_t v):value_(v){}
storage::SqlParam::SqlParam(uint64_t v):value_(v){}
storage::SqlParam::SqlParam(double v):value_(v){}
storage::SqlParam::SqlParam(bool v):value_(v){}

void storage::SqlParam::bind(sql::PreparedStatement* stmt,int index)const{
    std::visit([&](const auto& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,std::string>){
            stmt->setString(index,v);
        }
        else if constexpr(std::is_same_v<T,int64_t>){
            stmt->setInt64(index,v);
        }
        else if constexpr(std::is_same_v<T,uint64_t>){
            stmt->setUInt64(index,v);
        }
        else if constexpr(std::is_same_v<T,double>){
            stmt->setDouble(index,v);
        }
        else if constexpr(std::is_same_v<T,bool>){
            stmt->setBoolean(index,v);
        }
    },value_);
}