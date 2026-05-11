#pragma once 
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
/*
统一SQL执行结果
屏蔽MySQL Connector/C++的原生类型
Repo层只处理SqlResult,不直接碰数据库库对象
*/
namespace storage{
using SqlRow=std::unordered_map<std::string,std::string>;
class SqlResult{
public:
    bool success{true};//SQL是否成功
    std::string error;//失败原因
    std::vector<SqlRow> rows;//查询结果
    uint64_t affectedRows{0};//影响行数
    uint64_t lastInsertId{0};//插入后ID
    bool ok()const{return success;}
};
}