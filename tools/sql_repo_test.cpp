#include "config/AppConfig.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/RepositoryBundle.h"
#include "logger/LogMacros.h"
#include "storage/sql/SqlUserRepo.h"
#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlMessageRepo.h"

int main(){
    auto config=AppConfig::loadFromFile("config/config.json");
    config.applyEnvOverrides();
    config.validateOrThrow();
    LOG_INFO("Config summary: "+config.dumpSummary());
    auto pool=std::make_shared<storage::SqlConnectionPool>(config.database());
    if(!pool->start()){
        LOG_ERROR("Failed to start SQL connection pool");
        return -1;
    }
    if(!pool->healthy()){
        LOG_ERROR("SqlConnection is not healthy");
        return -1;
    }
    storage::RepositoryBundle bundle;
    bundle.userRepo=std::make_shared<storage::SqlUserRepo>(pool);
    bundle.groupRepo=std::make_shared<storage::SqlGroupRepo>(pool);
    bundle.messageRepo=std::make_shared<storage::SqlMessageRepo>(pool);
    if(!bundle.valid()){
        LOG_ERROR("Failed to create RepositoryBundle");
        return -1;
    }
    //测试用户创建
    auto userId=bundle.userRepo->createUser("testuser");
    if(!userId.ok()){
        LOG_ERROR("Failed to create user: "+userId.message);
        return -1;
    }
    LOG_INFO("Created user with ID: ");
    bool exists=bundle.userRepo->userExists("testuser");
    LOG_INFO("User exists: "+std::string(exists?"true":"false"));
    //测试群组创建
    auto groupId=bundle.groupRepo->createGroup("testgroup","Test Group","testuser");
    if(!groupId.ok()){
        LOG_ERROR("Failed to create group: "+groupId.message);
        return -1;
    }
    LOG_INFO("Created group with ID: ");
    exists=bundle.groupRepo->groupExists("testgroup");
    LOG_INFO("Group exists: "+std::string(exists?"true":"false"));
    //测试添加成员
    auto addMemberResult=bundle.groupRepo->addMember("testgroup","testuser");
    if(!addMemberResult.ok()){
        LOG_ERROR("Failed to add member: "+addMemberResult.message);
        return -1;
    }
    LOG_INFO("Added member to group");
    //测试列出成员
    auto members=bundle.groupRepo->listMembers("testgroup");
    LOG_INFO("Group members: ");
    for(const auto& member:members){
        LOG_INFO(" - "+member);
    }
    //测试保存消息
    auto saveResult=bundle.messageRepo->saveGroupMessage(1,"testgroup","testuser","Hello, World!",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    if(!saveResult.ok()){
        LOG_ERROR("Failed to save message: "+saveResult.message);
        return -1;
    }
    LOG_INFO("Message saved successfully");
    //测试列出消息
    auto messages=bundle.messageRepo->listGroupMessages("testgroup",0,10);
    LOG_INFO("Group messages: ");
    for(const auto& msg:messages){
        LOG_INFO(" - ["+std::to_string(msg.messageId)+"] "+msg.from+": "+msg.content);
    }
}