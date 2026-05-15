#include "storage/RepositoryFactory.h"
#include "storage/sql/SqlUserRepo.h"
#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlMessageRepo.h"
#include "storage/memory/MemoryUserRepo.h"
#include "storage/memory/MemoryGroupRepo.h"
#include "storage/memory/MemoryMessageRepo.h"

storage::RepositoryBundle storage::RepositoryFactory::createMemory(){
    RepositoryBundle bundle;
    bundle.userRepo=std::make_shared<MemoryUserRepo>();
    bundle.groupRepo=std::make_shared<MemoryGroupRepo>();
    bundle.messageRepo=std::make_shared<MemoryMessageRepo>();
    return bundle;
}
storage::RepositoryBundle storage::RepositoryFactory::createSql(const DatabaseConfig& dbConfig){
    auto pool=std::make_shared<SqlConnectionPool>(dbConfig);
    if(!pool->start()){
        std::runtime_error("Failed to start SQL connection pool");
    }
    RepositoryBundle bundle;
    bundle.userRepo=std::make_shared<SqlUserRepo>(pool);
    bundle.groupRepo=std::make_shared<SqlGroupRepo>(pool);
    bundle.messageRepo=std::make_shared<SqlMessageRepo>(pool);
    return bundle;
}
storage::RepositoryBundle storage::RepositoryFactory::create(const AppConfig& config){
    if(config.
}