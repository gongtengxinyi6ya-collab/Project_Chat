#pragma once
#include <string>
#include <set>
#include "third_party/json.hpp"
#include "ConfigParseHelper.h"
#include "logger/LogLevel.h"
/*决定输出目标和格式*/
class LogConfig{
public:
    static LogConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;
    LogLevel parseLogLevel(const nlohmann::json&,const std::string&);//映射字符串
    //属性
    LogLevel level{LogLevel::INFO};//默认日志级别INFO，合法值TRACE,DEBUG,INFO,WARN,ERROR
    bool toConsole{true};//默认输出到控制台
    bool toFile{false};//默认不输出到文件
    std::string filePath{"build/chat.log"};//默认日志文件路径，只有toFile为true时有效
    bool jsonFormat{false};//默认文本格式，设置为true时输出JSON格式日志，便于日志收集系统解析
};