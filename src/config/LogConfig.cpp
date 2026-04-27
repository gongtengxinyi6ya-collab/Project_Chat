#include "config/LogConfig.h"

LogConfig LogConfig::fromJson(const nlohmann::json& j){
    LogConfig logConfig;
    if(j.contains("level")){
        logConfig.level=logConfig.parseLogLevel(j,"level");
    }
    logConfig.toConsole=ConfigParseHelper::getOrDefault(j,"to_console",logConfig.toConsole);
    logConfig.toFile=ConfigParseHelper::getOrDefault(j,"to_file",logConfig.toFile);
    logConfig.filePath=ConfigParseHelper::getOrDefault(j,"file_path",logConfig.filePath);
    logConfig.jsonFormat=ConfigParseHelper::getOrDefault(j,"json_format",logConfig.jsonFormat);
    logConfig.asyncEnabled=ConfigParseHelper::getOrDefault(j,"async_enabled",logConfig.asyncEnabled);
    logConfig.asyncQueueSize=ConfigParseHelper::getOrDefault(j,"async_queue_size",logConfig.asyncQueueSize);
    logConfig.asyncFlushIntervalMs=ConfigParseHelper::getOrDefault(j,"async_flush_interval_ms",logConfig.asyncFlushIntervalMs);
    return logConfig;
}
void LogConfig::applyEnvOverrides(){
    auto envLevel=ConfigParseHelper::getEnv("LOG_LEVEL"); 
    if(envLevel.has_value()){
        level=parseLogLevel(nlohmann::json{{"level", envLevel.value()}}, "level");
    }
    auto envToConsole=ConfigParseHelper::getEnv("LOG_TO_CONSOLE");
    if(envToConsole.has_value()){
        toConsole=ConfigParseHelper::parseEnvBool(envToConsole.value(), "LOG_TO_CONSOLE");
    }
    auto envToFile=ConfigParseHelper::getEnv("LOG_TO_FILE");
    if(envToFile.has_value()){
        toFile=ConfigParseHelper::parseEnvBool(envToFile.value(), "LOG_TO_FILE");
    }
    auto envFilePath=ConfigParseHelper::getEnv("LOG_FILE_PATH");
    if(envFilePath.has_value()){
        filePath=envFilePath.value();
    }
    auto envJsonFormat=ConfigParseHelper::getEnv("LOG_JSON_FORMAT");
    if(envJsonFormat.has_value()){
        jsonFormat=ConfigParseHelper::parseEnvBool(envJsonFormat.value(), "LOG_JSON_FORMAT");
    }
    auto envAsyncEnabled=ConfigParseHelper::getEnv("LOG_ASYNC_ENABLED");
    if(envAsyncEnabled.has_value()){
        asyncEnabled=ConfigParseHelper::parseEnvBool(envAsyncEnabled.value(), "LOG_ASYNC_ENABLED");
    }
    auto envAsyncQueueSize=ConfigParseHelper::getEnv("LOG_ASYNC_QUEUE_SIZE");
    if(envAsyncQueueSize.has_value()){
        asyncQueueSize=ConfigParseHelper::parseEnvUInt(envAsyncQueueSize.value(), "LOG_ASYNC_QUEUE_SIZE");
    }
    auto envAsyncFlushIntervalMs=ConfigParseHelper::getEnv("LOG_ASYNC_FLUSH_INTERVAL_MS");
    if(envAsyncFlushIntervalMs.has_value()){
        asyncFlushIntervalMs=ConfigParseHelper::parseEnvUInt(envAsyncFlushIntervalMs.value(), "LOG_ASYNC_FLUSH_INTERVAL_MS");
    }
}
void LogConfig::validateOrThrow() const{
    static const std::set<LogLevel> validLevels{LogLevel::TRACE, LogLevel::DEBUG, LogLevel::INFO, LogLevel::WARN, LogLevel::ERROR, LogLevel::FATAL};
    if(validLevels.find(level)==validLevels.end()){
        throw std::runtime_error("Invalid log level: " + std::to_string(static_cast<int>(level)));
    }
    if(toFile&&!filePath.empty()){
        //简单校验路径合法性，不能包含非法字符
        if(filePath.find_first_of("<>:\"|?*")!=std::string::npos){
            throw std::runtime_error("Invalid log file path: "+filePath);
        }
    }
    if(asyncEnabled){
        if(asyncQueueSize==0){
            throw std::runtime_error("Async queue size must be greater than 0");
        }
        if(asyncFlushIntervalMs==0){
            throw std::runtime_error("Async flush interval must be greater than 0");
        }
    }
}
LogLevel LogConfig::parseLogLevel(const nlohmann::json& j,const std::string& key){
    std::string levelStr=ConfigParseHelper::getOrThrow<std::string>(j,key);
    if(levelStr=="TRACE"){
        return LogLevel::TRACE;
    }else if(levelStr=="DEBUG"){
        return LogLevel::DEBUG;
    }else if(levelStr=="INFO"){
        return LogLevel::INFO; 
    }else if(levelStr=="WARN"){
        return LogLevel::WARN;
    }else if(levelStr=="ERROR"){
        return LogLevel::ERROR;
    }else if(levelStr=="FATAL"){
        return LogLevel::FATAL;
    }
    else{
        throw std::runtime_error("Invalid log level string for config key "+key+": "+levelStr);
    }
}