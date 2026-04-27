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
#include "AsyncLogger.h"
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

    //异步日志接口
    void setAsync(bool enabled);
    void setAsyncOptions(size_t ququeSize,std::chrono::milliseconds);
    void shutdown();

private:
    Logger()=default;//默认构造函数
    Logger(const Logger&)=delete;//禁止拷贝构造
    Logger& operator=(const Logger&)=delete;//禁止拷贝赋值
    void writeLine(std::string&&);

    std::atomic<LogLevel> minLevel_{LogLevel::DEBUG};//最小输出级别
    bool incldeSource_{true};//是否打印
    std::unique_ptr<LogSink> sink_;//输出目标
    std::mutex mutex_;//同步写保护

    //异步日志接口
    bool asyncEnabled_{false};
    std::unique_ptr<AsyncLogger> asynclogger_;
    size_t asyncQueueSize_{10000};//异步日志队列上限
    std::chrono::milliseconds asyncFlushInterval_{100};//周期刷盘间
};