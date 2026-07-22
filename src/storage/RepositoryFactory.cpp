#include "storage/RepositoryFactory.h"
#include "logger/LogMacros.h"
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
#include "storage/sql/SqlGroupJoinRequestRepo.h"
#include "storage/sql/SqlGroupMessageWriteStore.h"
#include "storage/sql/SqlDirectMessageWriteStore.h"
storage::RepositoryBundle storage::RepositoryFactory::createSql(const DatabaseConfig& dbConfig){
    //创建通用连接池
    SqlConnectionPoolOptions commonOptions{
        .name="common",
        .poolSize=dbConfig.poolSize(),
        .acquireTimeout=std::chrono::milliseconds(dbConfig.acquireTimeoutMs()),
        .statementCacheSize=dbConfig.preparedStatementCacheSize()};
    auto commonPool=std::make_shared<SqlConnectionPool>(dbConfig,commonOptions);

    //创建消息连接池
    SqlConnectionPoolOptions messageOptions{
    .name = "message",
    .poolSize = dbConfig.messagePoolSize(),
    .acquireTimeout =std::chrono::milliseconds(dbConfig.messageAcquireTimeoutMs()),
    .statementCacheSize =dbConfig.preparedStatementCacheSize()
};
    auto messagePool=std::make_shared<SqlConnectionPool>(dbConfig,messageOptions);
    if(!commonPool->start()){
        throw std::runtime_error("Failed to start common SQL connection pool");
    }
    if(!messagePool->start()){
        commonPool->stop();
        throw std::runtime_error("Failed to start message SQL connection pool");
    }
    if (!commonPool->healthy() ||!messagePool->healthy()) {
        messagePool->stop();
        commonPool->stop();
        throw std::runtime_error("SQL pool health check failed");
    }
    RepositoryBundle bundle;
    //通用池注入
    bundle.userRepo=std::make_shared<SqlUserRepo>(commonPool);
    bundle.groupRepo=std::make_shared<SqlGroupRepo>(commonPool);
    bundle.userSessionRepo=std::make_shared<SqlUserSessionRepo>(commonPool);
    bundle.userProfileRepo=std::make_shared<SqlUserProfileRepo>(commonPool);
    bundle.friendRepo=std::make_shared<SqlFriendRepo>(commonPool);
    bundle.friendRequestRepo=std::make_shared<SqlFriendRequestRepo>(commonPool);
    bundle.groupJoinRequestRepo=std::make_shared<SqlGroupJoinRequestRepo>(commonPool);
    bundle.sqlPool=commonPool;
    //消息池注入
    bundle.messageRepo=std::make_shared<SqlMessageRepo>(messagePool);
    bundle.offlineMessageRepo=std::make_shared<SqlOfflineMessageRepo>(messagePool);
    bundle.conversationRepo=std::make_shared<SqlConversationRepo>(messagePool);
    bundle.groupMessageWriteStore=std::make_shared<SqlGroupMessageWriteStore>(messagePool);
    bundle.directMessageWriteStore=std::make_shared<SqlDirectMessageWriteStore>(messagePool);
    bundle.messageSqlPool=messagePool;


    auto commonPoolStats = commonPool->stats();
    auto messagePoolStats=messagePool->stats();
    LOG_INFO("common SQL pool started total=" + std::to_string(commonPoolStats.total) +
            " idle=" + std::to_string(commonPoolStats.idle) +
            " acquireTimeoutMs=" + std::to_string(commonPoolStats.acquireTimeoutMs)+
            " message SQL pool started total=" + std::to_string(messagePoolStats.total) +
            " idle=" + std::to_string(messagePoolStats.idle) +
            " acquireTimeoutMs=" + std::to_string(messagePoolStats.acquireTimeoutMs)
        );
    
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