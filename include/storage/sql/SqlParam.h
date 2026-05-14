#pragma once
#include <string>
#include <cstdint>
#include <variant>

/*统一表示SQL参数类型*/
namespace sql{
    class PreparedStatement;
}
namespace storage{
class SqlParam{
public:
    SqlParam(std::string v);
    SqlParam(const char*);
    SqlParam(int64_t v);
    SqlParam(uint64_t v);
    SqlParam(double v);
    SqlParam(bool v);
    void bind(sql::PreparedStatement* stmt,int index)const;//绑定参数到预编译语句
private:
    std::variant<std::string,int64_t,uint64_t,double,bool> value_;//保存参数值

};
}