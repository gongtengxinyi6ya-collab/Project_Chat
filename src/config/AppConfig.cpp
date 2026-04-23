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
        return config;
    }catch(const nlohmann::json::exception& e){
        throw std::runtime_error("Failed to parse config file: "+std::string(e.what()));
    }

}
void AppConfig::applyEnvOverriders(){
    server_.applyEnvOverrides();
    net_.applyEnvOverrides();
    log_.applyEnvOverrides();
    im_.applyEnvOverrides();
}
void AppConfig::validateOrThrow() const{
    server_.validateOrThrow();
    net_.validateOrThrow();
    log_.validateOrThrow();
    im_.validateOrThrow();  
}
std::string AppConfig::dumpSummary() const{
    std::stringstream ss;
    ss<<"Server(host="<<server_.host<<",port="<<server_.port<<",ioThreads="<<server_.ioThreads<<",backlog="<<server_.backlog<<"); "
      <<"Net(heartBeatMs="<<net_.heartBeatMs<<",idleTimeoutMs="<<net_.idleTimeoutMs<<",maxFrameLen="<<net_.maxFrameLen<<"); "
      <<"Log(level="<<log_.level<<",toConsole="<<log_.toConsole<<",toFile="<<log_.toFile<<",filePath="<<log_.filePath<<",jsonFormat="<<log_.jsonFormat<<"); "
      <<"IM(requireGroupIdForSend="<<im_.requireGroupIdForSend<<",maxGroupNameLen="<<im_.maxGroupNameLen<<",maxMessageLen="<<im_.maxMessageLen<<");";
    return ss.str();
}