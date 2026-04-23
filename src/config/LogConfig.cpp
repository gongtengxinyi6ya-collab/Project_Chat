#include "config/LogConfig.h"

LogConfig LogConfig::fromJson(const nlohmann::json& j){
    LogConfig logConfig;
    logConfig.level=ConfigParseHelper::getOrDefault(j,"level",logConfig.level);
    logConfig.toConsole=ConfigParseHelper::getOrDefault(j,"to_console",logConfig.toConsole);
    logConfig.toFile=ConfigParseHelper::getOrDefault(j,"to_ile",logConfig.toFile);
    logConfig.filePath=ConfigParseHelper::getOrDefault(j,"file_path",logConfig.filePath);
    logConfig.jsonFormat=ConfigParseHelper::getOrDefault(j,"json_format",logConfig.jsonFormat);
    return logConfig;
}
void LogConfig::applyEnvOverrides(){
    auto envLevel=ConfigParseHelper::getEnv("LOG_LEVEL"); 
    if(envLevel.has_value()){
        level=envLevel.value();
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
}
void LogConfig::validateOrThrow() const{
    static const std::set<std::string> validLevels{"TRACE","DEBUG","INFO","WARN","ERROR"};
    if(validLevels.find(level)==validLevels.end()){
        throw std::runtime_error("Invalid log level: "+level);
    }
    if(toFile&&!filePath.empty()){
        //简单校验路径合法性，不能包含非法字符
        if(filePath.find_first_of("<>:\"|?*")!=std::string::npos){
            throw std::runtime_error("Invalid log file path: "+filePath);
        }
    }
}