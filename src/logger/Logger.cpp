#include "logger/Logger.h"

Logger& Logger::instance(){
    static Logger instance;
    return instance;

}
void Logger::setLevel(LogLevel level){
    minLevel_.store(level,std::memory_order_relaxed);
}
void Logger::setSink(std::unique_ptr<LogSink> sink){
    if(asyncEnabled_.load(std::memory_order_acquire)){
        if(asynclogger_){//如果已经启用异步日志，先停止旧的异步日志器，再创建新的
            asynclogger_->stop();
        }
        asynclogger_=std::make_unique<AsyncLogger>(std::move(sink),asyncQueueSize_,asyncFlushInterval_);
        asynclogger_->start();
    }
    else{
        std::lock_guard lk(mutex_);
        sink_=std::move(sink);
    }
}
void Logger::log(LogLevel level,std::string_view msg,const char* file,int line,const char* func){
    if(level<minLevel_){//级别过滤
        return;
    }
    auto now=std::chrono::system_clock::now();
    //转化为time_t+毫秒部分
    time_t timeT=std::chrono::system_clock::to_time_t(now);
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000;
    //获取线程id
    uint64_t tid=std::hash<std::thread::id>{}(std::this_thread::get_id());
    //组装一行字符串:YYYY-mm-dd HH:MM:SS.mmm [LEVEL] [tid=...] msg
    std::string logLine;
    logLine.reserve(256);
    char timeBuf[32];
    std::strftime(timeBuf,sizeof(timeBuf),"%Y-%m-%d %H:%M:%S",std::localtime(&timeT));
    logLine.append(timeBuf);
    logLine.append("."+std::to_string(ms));
    std::string levelStr=" ["+std::string(logLevelToString(level))+"] ";
    logLine.append(levelStr);
    logLine.append("[tid="+std::to_string(tid)+"] ");
    logLine.append(msg);
    //若提供file和line,func 追加
    if(file){
        logLine.append(" (");
        logLine.append(file);
        if(line>0){
            logLine.append(":");
            logLine.append(std::to_string(line));
        }
        if(func){
            logLine.append(" ");
            logLine.append(func);
        }
        logLine.append(")");
    }
    logLine.append("\n");
    //输出
    writeLine(std::move(logLine));


}

void Logger::logWithContext(LogLevel level,std::string_view msg,const LogContext& ctx,const char* file,int line,const char* func){
    if(level<minLevel_){//低于最小级别直接return
        return;
    }
    std::string logLine;//拼接日志内容
    logLine.reserve(256);
    //时间基础头
    auto now=std::chrono::system_clock::now();
    auto s=std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-s).count();
    time_t timeT=std::chrono::system_clock::to_time_t(s);
    char timeBuffer[32];
    strftime(timeBuffer,sizeof(timeBuffer),"%Y-%m-%d %H:%M:%S",std::localtime(&timeT));

    //线程id
    uint64_t tid=std::hash<std::thread::id>{}(std::this_thread::get_id());

    //拼接
    logLine.append(timeBuffer);
    logLine.append("."+std::to_string(ms));
    logLine.append(" ["+std::string(logLevelToString(level))+"] ");
    logLine.append("[tid= "+std::to_string(tid)+"] ");
    logLine.append(msg);
    if(!ctx.empty()){
        logLine.append(" "+ctx.toKvString());
    }
    if(file){
        logLine.append(" (");
        logLine.append(file);
        if(line>0){

            logLine.append(": "+std::to_string(line)+" ");
        }
        if(func){
            logLine.append(func);
        }
        logLine.append(")");
    }
    logLine.append("\n");
    writeLine(std::move(logLine));

}
//异步日志接口
void Logger::setAsync(bool enable){
    asyncEnabled_.store(true,std::memory_order_release);
}
void Logger::setAsyncOptions(size_t queueSize,std::chrono::milliseconds flushInterval){
    asyncQueueSize_=queueSize;
    asyncFlushInterval_=flushInterval;
    //若asynclogger_已在运行，重建它，保留当前sink_配置
    if(asyncEnabled_.load(std::memory_order_acquire)){
        std::unique_ptr<LogSink> currentSink;
        {
            std::lock_guard lk(mutex_);
            currentSink=std::move(sink_);
        }
        if(asynclogger_){
            asynclogger_->stop();
        }
        asynclogger_=std::make_unique<AsyncLogger>(std::move(currentSink),asyncQueueSize_,asyncFlushInterval_);
        asynclogger_->start();
    }
}
void Logger::shutdown(){
    if(asyncEnabled_.load(std::memory_order_acquire)&&asynclogger_){
        asynclogger_->stop();
    }
    else{
        std::lock_guard lk(mutex_);
        if(sink_){
            sink_->flush();
        }
    }
}
void Logger::writeLine(std::string&& line){
    if(asyncEnabled_.load(std::memory_order_acquire)&&asynclogger_){
        asynclogger_->append(std::move(line));
    }
    else{
        std::lock_guard lk(mutex_);
        if(sink_){
            sink_->write(line);
            sink_->flush();
        }
    }
}

LoggerStats Logger::stats()const{
    if(!asyncEnabled_.load(std::memory_order_acquire)){
        return LoggerStats{.asyncEnabled=false};
    }
    if(asynclogger_){
        return LoggerStats{.asyncEnabled=true,.asyncRunning=asynclogger_->isRunning(),
        .written=asynclogger_->writtenCount(),
        .dropped=asynclogger_->droppedCount(),
        .queueSize=asynclogger_->queueSize()};
    }
}