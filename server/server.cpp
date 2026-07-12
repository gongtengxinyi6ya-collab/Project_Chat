#include "TcpServer.h"
#include "logger/Logger.h"
#include "logger/FileSink.h"
#include "logger/StderrSink.h"
#include "config/AppConfig.h"
#include "infra/signal/SignalHandler.h"
#include "EventLoop.h"
#include "logger/LogMacros.h"
#include <iostream>
#include <exception>
int main()
{
    int exitCode=0;
    try{
        AppConfig config=AppConfig::loadFromFile("config/config.json");
        config.applyEnvOverrides();
        config.validateOrThrow();
        std::cout<<config.dumpSummary()<<std::endl;
        //根据配置设置日志系统
        Logger::instance().setLevel(config.log().level);
        Logger::instance().setAsyncOptions(config.log().asyncQueueSize,std::chrono::milliseconds(config.log().asyncFlushIntervalMs));
        Logger::instance().setAsync(config.log().asyncEnabled);
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
        {
            EventLoop loop;
            TcpServer server(&loop,config.server().port,config);
            server.setQuitCallback([&loop](){
                loop.quit();
            });
            
            infra::signal::SignalHandler signalHandler(&loop);
            signalHandler.setSignalCallback([&](int signo){
                LOG_WARN("received signal " + std::to_string(signo) + ", stopping server");
                server.stop();
            });
            signalHandler.start();

            server.setThreadNum(config.server().ioThreads);
            server.start();
            LOG_INFO("Server started on port " + std::to_string(config.server().port));
            loop.loop();
        }
        
    }catch(const std::exception& e){
        LOG_ERROR("Server start/run failed: " + std::string(e.what()));
        Logger::instance().shutdown();
        exitCode=1;
    }catch(...){
        LOG_ERROR("Server stopped because of an unknown exception");
        Logger::instance().shutdown();
        exitCode=1;
    }
    Logger::instance().shutdown();
    return exitCode;
}
    