#include "storage/RepositoryFactory.h"
#include "storage/memory/MemoryUserRepo.h"
#include "storage/memory/MemoryGroupRepo.h"
#include "storage/memory/MemoryMessageRepo.h"

#ifdef PROJECT_CHAT_ENABLE_SQL
#include "storage/sql/SqlUserRepo.h"
#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlMessageRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlOfflineMessageRepo.h"
#include "storage/sql/SqlUserSessionRepo.h"
#include "storage/sql/SqlUserProfileRepo.h"
#include "storage/sql/SqlFriendRepo.h"
#include "storage/sql/SqlFriendRequestRepo.h"
#include "storage/sql/SqlConversationRepo.h"
#include "storage/sql/SqLGroupJoinRequestRepo.h"
storage::RepositoryBundle storage::RepositoryFactory::createSql(const DatabaseConfig& dbConfig){
    auto pool=std::make_shared<SqlConnectionPool>(dbConfig);
    if(!pool->start()){
        throw std::runtime_error("Failed to start SQL connection pool");
    }
    if(!pool->healthy()){
        throw std::runtime_error("SqlConnection is not healthy");
    }
    RepositoryBundle bundle;
    bundle.userRepo=std::make_shared<SqlUserRepo>(pool);
    bundle.groupRepo=std::make_shared<SqlGroupRepo>(pool);
    bundle.messageRepo=std::make_shared<SqlMessageRepo>(pool);
    bundle.offlineMessageRepo=std::make_shared<SqlOfflineMessageRepo>(pool);
    bundle.userSessionRepo=std::make_shared<SqlUserSessionRepo>(pool);
    bundle.userProfileRepo=std::make_shared<SqlUserProfileRepo>(pool);
    bundle.friendRepo=std::make_shared<SqlFriendRepo>(pool);
    bundle.friendRequestRepo=std::make_shared<SqlFriendRequestRepo>(pool);
    bundle.conversationRepo=std::make_shared<SqlConversationRepo>(pool);
    bundle.groupJoinRequestRepo=std::make_shared<SqlGroupJoinRequestRepo>(pool);
    return bundle;
}
#else
storage::RepositoryBundle storage::RepositoryFactory::createSql(const DatabaseConfig& config){
    throw std::runtime_error("SQL backend is disabled at build time");
}
#endif
storage::RepositoryBundle storage::RepositoryFactory::createMemory(){
    RepositoryBundle bundle;
    bundle.userRepo=std::make_shared<MemoryUserRepo>();
    bundle.groupRepo=std::make_shared<MemoryGroupRepo>();
    bundle.messageRepo=std::make_shared<MemoryMessageRepo>();
    return bundle;
}

storage::RepositoryBundle storage::RepositoryFactory::create(const AppConfig& config){
    if(config.storage().type()=="memory"){
        return createMemory();
    }
    if(config.storage().type()=="sql"){
        try{
            return createSql(config.database());
        }catch(const std::exception& e){
            if(config.storage().fallbackToMemory()){
                LOG_WARN("Failed to create sql,create memory instead"+std::string(e.what()));
                return createMemory();
            }
            else
                throw std::runtime_error("Failed to create Repository");
        }
    }
    if(config.storage().fallbackToMemory()){
        LOG_WARN("Failed to createsql ,createMemory instead");
        return createMemory();
    }
    
    throw std::runtime_error("Failed to create RepositoryBundle");
}