#include "config/AppConfig.h"
#include <fstream>
#include <sstream>

AppConfig AppConfig::loadFromFile(const std::string& path){
    std::ifstream file(path);
    if(!file.is_open()){
        throw std::runtime_error("Failed to open config file: "+path);
    }
    std::stringstream buffer;
    buffer<<file.rdbuf();
    try{
        auto j=nlohmann::json::parse(buffer.str());
        AppConfig config;
        config.server_=ServerConfig::fromJson(j.value("server", nlohmann::json::object()));
        config.net_=NetConfig::fromJson(j.value("net", nlohmann::json::object()));
        config.log_=LogConfig::fromJson(j.value("log", nlohmann::json::object()));
        config.im_=ImConfig::fromJson(j.value("im", nlohmann::json::object()));
        config.databaseConfig_=DatabaseConfig::fromJson(j.value("database",nlohmann::json::object()));
        config.storageConfig_=StorageConfig::fromJson(j.value("storage",nlohmann::json::object()));
        return config;
    }catch(const nlohmann::json::exception& e){
        throw std::runtime_error("Failed to parse config file: "+std::string(e.what()));
    }

}
void AppConfig::applyEnvOverrides(){
    server_.applyEnvOverrides();
    net_.applyEnvOverrides();
    log_.applyEnvOverrides();
    im_.applyEnvOverrides();
    databaseConfig_.loadFromEnv();
    storageConfig_.loadFromEnv();
}
void AppConfig::validateOrThrow() const{
    server_.validateOrThrow();
    net_.validateOrThrow();
    log_.validateOrThrow();
    im_.validateOrThrow();  
    databaseConfig_.validate();
    storageConfig_.validateOrThrow();
}
std::string AppConfig::dumpSummary() const{
    std::stringstream ss;
    ss<<"Server(host="<<server_.host<<",port="<<server_.port<<",ioThreads="<<server_.ioThreads<<",backlog="<<server_.backlog<<"); "
      <<"Net(heartBeatMs="<<net_.heartBeatMs<<",maxFrameLen="<<net_.maxFrameLen<<"); "
      <<"Log(level="<<logLevelToString(log_.level)<<",toConsole="<<log_.toConsole<<",toFile="<<log_.toFile<<",filePath="<<log_.filePath<<",jsonFormat="<<log_.jsonFormat<<"); "
      <<"IM(requireGroupIdForSend="<<im_.requireGroupIdForSend<<",maxGroupNameLen="<<im_.maxGroupNameLen<<",maxMessageLen="<<im_.maxMessageLen<<");"
      <<"Database(host="<<databaseConfig_.host()<<",port="<<databaseConfig_.port()<<",user="<<databaseConfig_.user()<<",database="<<databaseConfig_.database()<<",poolSize="<<databaseConfig_.poolSize()<<",connectTimeoutMs="<<databaseConfig_.connectTimeoutMs()<<")"
      <<"Storage(type="<<storageConfig_.type()<<",fallbackToMemory="<<storageConfig_.fallbackToMemory()<<"); ";
    return ss.str();
}