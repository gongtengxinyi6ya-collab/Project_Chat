#pragma once
#include <string>
#include <cstdint>
#include "third_party/json.hpp"
#include "ConfigParseHelper.h"
class DatabaseConfig{
public:
    static DatabaseConfig fromJson(const nlohmann::json& j);//读取配置
    void loadFromEnv();//环境变量覆盖配置
    void validate()const;//变量范围检查
    const std::string& host()const;//获取数据库地址
    uint16_t port()const;
    const std::string& user()const;
    const std::string& password()const;
    const std::string& database()const;
    uint32_t poolSize()const;
    uint32_t connectTimeoutMs()const;

private:
    std::string host_{"127.0.0.1"};//数据库地址
    uint16_t port_{3306};//MySQL默认端口
    std::string user_{"root"};//数据库用户名
    std::string password_;//数据库密码
    std::string database_{"project_chat"};//使用哪个数据库
    uint32_t poolSize_{4};//数据库连接池大小
    uint32_t connectTimeoutMs_{3000};//数据库连接超时时间
};