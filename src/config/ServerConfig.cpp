#include "config/ServerConfig.h"
#include <fstream>
#include <cstdlib>
#include <sstream>
ServerConfig ServerConfig::fromJson(const nlohmann::json&j){
    ServerConfig servercongig;
    servercongig.host=ConfigParseHelper::getOrDefault(j,"host",servercongig.host);
    servercongig.port=ConfigParseHelper::getOrDefault(j,"port",servercongig.port);
    servercongig.ioThreads=ConfigParseHelper::getOrDefault(j,"io_threads",servercongig.ioThreads);
    servercongig.backlog=ConfigParseHelper::getOrDefault(j,"backlog",servercongig.backlog);
    servercongig.backgroundThreads=ConfigParseHelper::getOrDefault(j,"background_threads",servercongig.backgroundThreads);
    servercongig.backgroundQueueCapacity=ConfigParseHelper::getOrDefault(j,"background_queue_capacity",servercongig.backgroundQueueCapacity);
    
    return servercongig;
}
void ServerConfig::applyEnvOverrides(){
    auto envHost=ConfigParseHelper::getEnv("SERVER_HOST");
    if(envHost.has_value()){
        host=envHost.value();
    }
    auto envPort=ConfigParseHelper::getEnv("SERVER_PORT");
    if(envPort.has_value()){
        port=ConfigParseHelper::parseEnvUInt(envPort.value(), "SERVER_PORT");
    }
    auto envIoThreads=ConfigParseHelper::getEnv("SERVER_IO_THREADS");
    if(envIoThreads.has_value()){
        ioThreads=ConfigParseHelper::parseEnvInt(envIoThreads.value(), "SERVER_IO_THREADS");
    }
    auto envBacklog=ConfigParseHelper::getEnv("SERVER_BACKLOG");
    if(envBacklog.has_value()){
        backlog=ConfigParseHelper::parseEnvUInt(envBacklog.value(), "SERVER_BACKLOG");
    }
    auto envBackthread=ConfigParseHelper::getEnv("SERVER_BACKGROUND_THREADS");
    if(envBackthread.has_value()){
        backgroundThreads=ConfigParseHelper::parseEnvUInt64(envBackthread.value(), "SERVER_BACKGROUND_THREADS");
    }
    auto envBackqueue=ConfigParseHelper::getEnv("SERVER_BACKGROUND_QUEUE_CAPACITY");
    if(envBackqueue.has_value()){
        backgroundQueueCapacity=ConfigParseHelper::parseEnvUInt64(envBackqueue.value(), "SERVER_BACKGROUND_QUEUE_CAPACITY");
    }
}
void ServerConfig::validateOrThrow() const{
    ConfigParseHelper::checkRange("port", port, 1, 65535);
    ConfigParseHelper::checkRange("ioThreads", ioThreads, 0, 1000);
    ConfigParseHelper::checkRange("backlog", backlog, 1, 65535);
    ConfigParseHelper::checkRange("background_threads",backgroundThreads,1,64);
    ConfigParseHelper::checkRange("background_queue_capacity",backgroundQueueCapacity,1,100000);
}