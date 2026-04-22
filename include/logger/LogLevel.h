#pragma once
//控制日志输出，线上关闭debug
enum class LogLevel{
    TRACE,//最详细的日志，包含函数调用、变量值等调试信息，开发阶段使用
    DEBUG,//调试日志，包含重要的调试信息，如函数入口、参数值等，开发阶段使用
    INFO,//一般信息日志，记录系统运行状态、重要事件等，生产环境使用
    WARN,//警告日志，记录潜在问题、异常情况等，生产环境使用
    ERROR,//错误日志，记录错误信息、异常堆栈等，生产环境使用
    FATAL//致命日志，记录导致程序崩溃的严重错误，生产环境使用
};

constexpr const char* logLevelToString(LogLevel level){
    switch(level){
        case LogLevel::TRACE:
            return "TRACE";
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}