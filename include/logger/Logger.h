#pragma once
#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>
#include "LogLevel.h"
#include "LogSink.h"
#include "LogContext.h"
/*提供log(level,msg,file,line,func)接口
生成一条完整日志行
交给Sink输出（文件，控制台）
维护全局配置
*/

class Logger{
public:
    static Logger& instance();//全局单例
    void setLevel(LogLevel level);//设置级别
    void setSink(std::unique_ptr<LogSink> sink);//切换输出目标
    void log(LogLevel level,std::string_view msg,const char* file=nullptr,int line=0,const char* func=nullptr);//生成一条完整日志并输出
    void logWithContext(LogLevel level,std::string_view msg,const LogContext& ctx,const char* file=nullptr,int line=0,const char* func=nullptr);//输出标准日志头+业务上下文字段


private:
    Logger()=default;//默认构造函数
    Logger(const Logger&)=delete;//禁止拷贝构造
    Logger& operator=(const Logger&)=delete;//禁止拷贝赋值

    std::atomic<LogLevel> minLevel_{LogLevel::DEBUG};//最小输出级别
    bool incldeSource_{true};//是否打印
    std::unique_ptr<LogSink> sink_;//输出目标
    std::mutex mutex_;//同步写保护

};