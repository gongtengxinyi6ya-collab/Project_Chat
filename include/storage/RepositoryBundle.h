#pragma once
#include<memory>
/*统一注入储存依赖*/
namespace storage{

class UserRepo;
class GroupRepo;
class MessageRepo;
class OfflineMessageRepo;
class UserSessionRepo;
class UserProfileRepo;
class FriendRepo;
class FriendRequestRepo;
class ConversationRepo;
class GroupJoinRequestRepo;
class GroupMessageWriteStore;
class SqlConnectionPool;
class DirectMessageWriteStore;

class RepositoryBundle{
public:
    std::shared_ptr<UserRepo> userRepo;
    std::shared_ptr<GroupRepo> groupRepo;
    std::shared_ptr<MessageRepo> messageRepo;
    std::shared_ptr<OfflineMessageRepo> offlineMessageRepo;//注入离线消息存储
    std::shared_ptr<UserSessionRepo> userSessionRepo;//注入用户会话存储
    std::shared_ptr<UserProfileRepo> userProfileRepo;//注入用户资料存储
    std::shared_ptr<FriendRepo> friendRepo;//好友关系存储
    std::shared_ptr<FriendRequestRepo> friendRequestRepo;//好友申请存储
    std::shared_ptr<ConversationRepo> conversationRepo;
    std::shared_ptr<GroupJoinRequestRepo> groupJoinRequestRepo;
    std::shared_ptr<GroupMessageWriteStore> groupMessageWriteStore;//群消息核心事务接口
    std::shared_ptr<DirectMessageWriteStore> directMessageWriteStore;//注入私聊事务存储
    
    std::shared_ptr<SqlConnectionPool> sqlPool;
    std::shared_ptr<SqlConnectionPool> messageSqlPool;//消息独立线程池

    bool valid()const{return userRepo&&groupRepo&&messageRepo;};
    bool hasSqlPool()const{return sqlPool!=nullptr;}
    void shutdown();
    bool hasMessageSqlPool() const noexcept {return messageSqlPool != nullptr;}
};
}