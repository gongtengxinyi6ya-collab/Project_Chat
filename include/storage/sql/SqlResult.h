#pragma once 
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <optional>
#include <cstdint>
#include <cstdlib>
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
    std::string error{};//失败原因
    std::vector<SqlRow> rows{};//查询结果
    uint64_t affectedRows{0};//影响行数
    uint64_t lastInsertId{0};//插入后ID
    int errorCode{0};//保存MySQL原始错误码
    std::string sqlState{};//保存SQL标准状态码，区分连接异常、约束异常
    bool ok()const{return success;}
};

inline std::string getString(
    const SqlRow& row,
    const std::string& key,
    const std::string& defaultValue = ""
) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty() || it->second == "NULL") {
        return defaultValue;
    }
    return it->second;
}

inline uint64_t getUInt64(
    const SqlRow& row,
    const std::string& key,
    uint64_t defaultValue = 0
) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty() || it->second == "NULL") {
        return defaultValue;
    }

    try {
        return std::stoull(it->second);
    } catch (...) {
        return defaultValue;
    }
}

inline int64_t getInt64(
    const SqlRow& row,
    const std::string& key,
    int64_t defaultValue = 0
) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty() || it->second == "NULL") {
        return defaultValue;
    }

    try {
        return std::stoll(it->second);
    } catch (...) {
        return defaultValue;
    }
}

inline int getInt(
    const SqlRow& row,
    const std::string& key,
    int defaultValue = 0
) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty() || it->second == "NULL") {
        return defaultValue;
    }

    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultValue;
    }
}
}