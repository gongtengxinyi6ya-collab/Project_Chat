#pragma once 
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
/*
屏蔽MySQL Connector/C++的原生类型
Repo层只处理SqlResult,不直接碰数据库库对象
*/
namespace{
using SqlRow=std::unordered_map<std::string,std::string>;
class SqlResult{
public:
    bool success{true};
    std::string error;
    std::vector<SqlRow> rows;
    uint64_t affectedRows{0};
    uint64_t lastInsertId{0};
    bool ok()const{return success;}
};
}