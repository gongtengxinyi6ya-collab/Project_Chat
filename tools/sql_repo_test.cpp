#include "config/AppConfig.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/RepositoryBundle.h"
#include "logger/LogMacros.h"
#include "logger/Logger.h"
#include "logger/FileSink.h"
#include "logger/StderrSink.h"
#include "storage/sql/SqlUserRepo.h"
#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlMessageRepo.h"
#include "security/PasswordHasher.h"
#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlUserSessionRepo.h"
#include "storage/sql/SqlUserProfileRepo.h"
#include "auth/AuthService.h"


int main(){
    auto config=AppConfig::loadFromFile("config/config.json");
    config.applyEnvOverrides();
    config.validateOrThrow();
    LOG_INFO("Config summary: "+config.dumpSummary());
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

    
}