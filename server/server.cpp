#include "TcpServer.h"
#include "logger/Logger.h"
#include "logger/FileSink.h"
#include "logger/StderrSink.h"
#include "config/AppConfig.h"
#include <iostream>
int main()
{
    AppConfig config=AppConfig::loadFromFile("config/config.json");
    config.applyEnvOverrides();
    config.validateOrThrow();
    std::cout<<config.dumpSummary()<<std::endl;
    //根据配置设置日志系统
    Logger::instance().setLevel(config.log().level);
    if(config.log().toFile){
        try{
            Logger::instance().setSink(std::make_unique<FileSink>(config.log().filePath,config.log().jsonFormat));
        }catch(const std::exception& e){
            std::cerr<<"Failed to initialize file sink: "<<e.what()<<", falling back to stderr"<<std::endl;
            Logger::instance().setSink(std::make_unique<StderrSink>());
        }
    }else{
        Logger::instance().setSink(std::make_unique<StderrSink>());
    }

    EventLoop loop;
    TcpServer server(&loop,config.server().port,config);
    server.setThreadNum(config.server().ioThreads);
    server.start();
    LOG_INFO("Server started on port " + std::to_string(config.server().port));
    loop.loop();
    return 0;
}
    