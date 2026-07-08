#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"

#include "storage/UserRepo.h"
#include "storage/GroupRepo.h"
#include "storage/MessageRepo.h"
#include "storage/GroupJoinRequestRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "storage/UserProfileRepo.h"
#include "storage/FriendRepo.h"

#include "auth/AuthService.h"
#include "auth/AuthResult.h"

#include "im/FriendService.h"
#include "im/ConversationService.h"
#include "common/ConversationKey.h"

#include "im/MessageSyncService.h"
#include "im/MessageAckService.h"
#include "im/GroupService.h"
#include "im/GroupJoinService.h"

#include "security/rate_limit/RateLimiter.h"
#include "security/PasswordHasher.h"

#include "logger/LogMacros.h"

im::Imservice::Imservice(uint32_t supportedVer,const ImConfig& config,const IdConfig& idconfig):supportedVer_(supportedVer),imConfig_(config),idConfig_(idconfig),idGenerator_(idConfig_.snowflakeNodeId,idConfig_.snowflakeEpochMs){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
im::Imservice::~Imservice()=default;
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto &session=sessionManager_.getOrCreate(key);
    if (session.peerIp_.empty()) {
        session.peerIp_ = conn->peerIp();
        session.peerPort_ = conn->peerPort();
    }
    auto req_or_resp=im::tryParse(payload);
    if(auto req_ptr=std::get_if<im::Request>(&req_or_resp)){
        LOG_INFO_CTX("im request in",makeReqCtx(key,*req_ptr,session,"REQ_IN"));
        Response resp;
        try{
            resp=dispatcResqest(*req_ptr,key,session);
        }catch(const std::exception& e){
            resp=makeErr(*req_ptr,im::ErrorCode::INTERNAL,"Internal server error:"+std::string(e.what()));
            //记录DISPATCH_EXCEPTION日志，包含请求上下文和异常信息
            LOG_ERROR_CTX("Exception occurred while dispatching request",makeReqCtx(key,*req_ptr,session,"DISPATCH_EXCEPTION"));

        }catch(...){
            resp=makeErr(*req_ptr,im::ErrorCode::INTERNAL,"Internal server error");
            LOG_ERROR_CTX("Unknown exception occurred while dispatching request",makeReqCtx(key,*req_ptr,session,"DISPATCH_EXCEPTION"));
        }
        sendResponseWithLog(key,*req_ptr,resp,session,"RESP_OUT");

    }
    else if(auto resp_ptr=std::get_if<im::Response>(&req_or_resp)){
        sendParseErrorWithLog(key,*resp_ptr,session);
    }
    
}
void im::Imservice::onDisconnect(const std::shared_ptr<TcpConnection> & conn){
    ConnKey key=conn->fd();
    sessionManager_.unbindConn(key);
    sessionManager_.erase(key);
}


im::Response im::Imservice::handleAuth(const Request&req,ConnKey key,Session& session){
    if(!imConfig_.allowDebugAuth){
        return makeErr(req,ErrorCode::BAD_REQUEST,"NOT be allowed to auth,please login first");
    }
    if(!req.body.contains("user")){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing username field");
    }
    std::string username=req.body["user"].get<std::string>();
    if(username.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"username cannot be empty");
    }
    if(session.state_==im::ConnState::Authed&&session.username_==username){//同一连接上已经认证过且用户名相同，幂等处理直接返回成功
        return makeOk(req,im::MsgType::AUTH_RESP);
    }
    if(session.state_==im::ConnState::Authed&&session.username_!=username){//同一连接上已经认证过但用户名不同，拒绝
        return makeErr(req,im::ErrorCode::USER_EXISTS,"User already authenticated with a different username");
    }
    std::string accountId;
    auto getAccountId=getStringField(req,"accountId",accountId);
    if(getAccountId.has_value()){
        return getAccountId.value();
    }
    uint64_t userId;
    if(req.body.contains("userId")&&req.body["userId"].is_number_unsigned()){
        userId=req.body["userId"].get<uint64_t>();
    }
    if(!sessionManager_.bindUser(key,userId,accountId,username)){
        return makeErr(req,im::ErrorCode::INTERNAL,"Failed to bind user to session");
    }
    /*
    if(hasRepositories()){
        auto result=repos_.userRepo->createUser(username);
        if(result.status!=storage::RepoStatus::Ok&&result.status!=storage::RepoStatus::AlreadyExists){
            return makeRepoError(req,result.status,"Fail to create user");
        }
    }
    */
    return makeOk(req,im::MsgType::AUTH_RESP);
}

std::optional<im::Response> im::Imservice::guardAuthenticated(const Request& req,const Session& session){
    if(session.state_==im::ConnState::Authed){
        return std::nullopt;
    }
    return makeErr(req,im::ErrorCode::NOT_AUTHED,"Unauthed, please authenticate first");
}
std::optional<im::Response> im::Imservice::guardInGroup(const Request& req,const Session& session,const std::string& groupId){
    if(!groupId.empty()&&groupManager_.isMember(groupId,session.accountId_)){
        return std::nullopt;
    }
    return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"Not in group,please join the group first");
}
std::optional<im::Response> im::Imservice::getStringField(const Request&req,const std::string& field,std::string& out,bool allowEmpty){
    if(!req.body.contains(field)){//不包含字段
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing field"+field);
    }
    if(!req.body[field].is_string()){//字段非字符串
        return makeErr(req,im::ErrorCode::BAD_REQUEST,"Bad request");
    }
    std::string str=req.body[field].get<std::string>();
    if(allowEmpty==false&&str.empty()){//字段为空
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Field is empty");
    }
    out=str;
    return std::nullopt;
}
std::string_view im::Imservice::sendResultToString(SendResult result)const{
    switch(result){
        case SendResult::Ok:
            return "Ok";
        case SendResult::NoSuchConnection:
            return "NoSuchConnection";
        case SendResult::Closed:
            return "Closed";
        case SendResult::Overloaded:
            return "Overloaded";
        default:
            return "Unknown";
    }
}

//持久化存储接口
void im::Imservice::setRepositories(storage::RepositoryBundle repos){
    repos_=std::move(repos);
    if(repos_.userRepo&&repos_.userSessionRepo&&repos_.userProfileRepo){
        security::PasswordHasher passwordHash(16,"SHA256");
        security::TokenManager tokenManager;
        authService_=std::make_unique<auth::AuthService>(repos_.userRepo,passwordHash,tokenManager,repos_.userSessionRepo,repos_.userProfileRepo);
    }
    if(repos_.friendRepo&&repos_.userProfileRepo&&repos_.friendRequestRepo){
        friendService_=std::make_unique<im::FriendService>(repos_.friendRepo,repos_.userProfileRepo,repos_.friendRequestRepo);
    }
    if(repos_.conversationRepo&&repos_.userProfileRepo&&repos_.groupRepo){
        conversationService_=std::make_unique<ConversationService>(repos_.conversationRepo,repos_.userProfileRepo,repos_.groupRepo);
    }
    if(repos_.messageRepo&&repos_.offlineMessageRepo){
        messageSyncService_=std::make_unique<MessageSyncService>(repos_.messageRepo,repos_.offlineMessageRepo);
    }
    if(repos_.conversationRepo&&repos_.messageRepo&&repos_.offlineMessageRepo){
        messageAckService_=std::make_unique<MessageAckService>(repos_.messageRepo,repos_.offlineMessageRepo,repos_.conversationRepo);
    }
    if(repos_.groupRepo&&repos_.userProfileRepo&&repos_.friendRepo){
        groupService_=std::make_unique<GroupService>(groupManager_,idGenerator_,repos_.groupRepo,repos_.userProfileRepo,repos_.friendRepo,imConfig_.requireFriendForGroupInvite,imConfig_.maxGroupMembers);
    }
    if(repos_.groupRepo&&repos_.userProfileRepo&&repos_.groupJoinRequestRepo){
        groupJoinService_=std::make_unique<GroupJoinService>(groupManager_,repos_.groupRepo,repos_.userProfileRepo,repos_.groupJoinRequestRepo,imConfig_.maxGroupMembers);
    }
}
void im::Imservice::setRateLimiter(std::unique_ptr<security::RateLimiter> limiter){
    rateLimiter_=std::move(limiter);
}

bool im::Imservice::hasRepositories()const{
    return repos_.valid();
}
void im::Imservice::loadFromRepositories(){
    if(!hasRepositories()||!repos_.groupRepo){
        return;
    }
    size_t restoreGroups=0;//统计恢复群数量
    size_t restoreMembers=0;//统计恢复成员数量
    size_t failedGroups=0;//统计群成员恢复失败数量
    std::vector<storage::GroupSnapshot> groups=repos_.groupRepo->listGroups();
    for(const auto& group:groups){
        auto members=repos_.groupRepo->listMemberRecords(group.groupId);
        if(groupManager_.restoreGroup(group.groupId,group.groupName,group.ownerAccountId,members)){
            restoreGroups++;
            restoreMembers+=members.size();
        }
        else{
            failedGroups++;
        }
    }
    LOG_INFO("Successfully restored Groups: "+std::to_string(restoreGroups)+"Successfully restored members: "+std::to_string(restoreMembers)+"Failed to restores groups: "+std::to_string(failedGroups));
}


im::Response im::Imservice::handleDm(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    //发消息限流
    if(rateLimiter_){
        auto limitResult=rateLimiter_->checkSendMessage(session.accountId_,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }
    }
    
    //取目标
    if(req.to.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message recipient");
    }
    //取文本
    std::string content;
    if(auto errField=getStringField(req,"content",content)){
        return errField.value();
    }
    if(!friendService_){
        return makeErr(req,im::ErrorCode::INTERNAL,"Friend service is not available");
    }
    auto userProfile=friendService_->findUser(req.to);
    if(!userProfile){
        return makeErr(req,ErrorCode::NO_SUCH_USER,"no such user");
    }
    //确认是否好友关系
    if(!friendService_->areFriends(session.accountId_,req.to)){
        return makeErr(req,ErrorCode::NOT_FRIENDS,"not your friends");
    }
    //生成消息id和时间
    uint64_t msgId=nextMessageId();
    uint64_t serverTsMs=nowMs();
    //保存私聊消息
    std::string conversationKey=common::buildDirectConversationKey(session.accountId_,req.to);
    if(!repos_.messageRepo){
       return makeErr(req,ErrorCode::INTERNAL,"messageRepo is not available");
    }
    auto result=repos_.messageRepo->saveDirectMessage(msgId,conversationKey,session.accountId_,req.to,session.username_,content,serverTsMs);
    if(!result.ok()){
        return makeErr(req,ErrorCode::INTERNAL,result.message);
    }
    //更新会话表
    if(conversationService_){
        auto resultConversation=conversationService_->recordDirectMessage(session.accountId_,req.to,session.username_,msgId,content,serverTsMs);
        if(!resultConversation.ok()){
            LOG_WARN("Failed to updated conversation for direct message");
        }
    }
    //构造推送消息
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::DM_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New direct message",.data=nlohmann::json{{"msgId",msgId},{"fromAccountId",session.accountId_},{"fromUsername",session.username_},{"toAccountId",req.to},{"content",content}}};
    auto pushResult=pushToAccount(req.to,pushMsg);
    if(pushResult.sent==0){//目标账号没有任何设备收到
        if(!repos_.offlineMessageRepo){
            return makeErr(req,ErrorCode::INTERNAL,"messageRepo is not available");
    }
        auto resultOffline=repos_.offlineMessageRepo->saveOfflineDirectMessage(req.to,msgId,session.accountId_);
        if(!resultOffline.ok()){
            LOG_WARN("Failed to save offlineMessage: "+content);
        }
        return makeOk(req,im::MsgType::DM_RESP,nlohmann::json{{"msgId",msgId},{"serverTsMs",serverTsMs},{"delivered",pushResult.delivered()},{"queuedOffline",true},{"sent",pushResult.sent},{"failed",pushResult.failed()}});
    }
    return makeOk(req,im::MsgType::DM_RESP,nlohmann::json{{"msgId",msgId},{"serverTsMs",serverTsMs},{"delivered",pushResult.delivered()},{"queuedOffline",false},{"sent",pushResult.sent},{"failed",pushResult.failed()}});

}

im::Response im::Imservice::handleListUsers(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::vector<std::string>users=sessionManager_.onLineUsers();
    return makeOk(req,im::MsgType::LIST_USERS_RESP,nlohmann::json{{"users",users}});
}

im::Response im::Imservice::handleEcho(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    return makeOk(req,im::MsgType::ECHO_RESP,req.body);
}
uint64_t im::Imservice::nowMs()const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
uint64_t im::Imservice::nextMessageId(){
    return nextMsgId_++;
}
void im::Imservice::decorate(im::Response& resp,std::optional<uint64_t> msgId,std::optional<uint64_t> clientReqId){
    if(msgId){
        resp.data["msg_id"]=msgId;
    }
    else{
        resp.data["msgId"]=nextMsgId_++;
    }
    resp.data["serverTsMs"]=nowMs();
    if(clientReqId.has_value()){
        resp.data["clientReqId"]=*clientReqId;
    }
}
 im::Imservice::SendResult im::Imservice::sendPush(ConnKey target,const std::string& payload){
    
    if(sendToConnKey_){
        SendResult res=sendToConnKey_(target,payload);
        return res;
    }
    return SendResult::NoSuchConnection;
    
}

//房间接口

im::Response im::Imservice::handleCreateGroup(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::string groupName;
    if(auto errField=getStringField(req,"groupName",groupName)){
        return errField.value();
    }
    if(groupName.size()>imConfig_.maxGroupNameLen){//群名称过长
        return makeErr(req,im::ErrorCode::GROUP_NAME_INVALID,"Group name is too long");
    }
    std::string owner=session.accountId_;
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }
    auto resultCreate=groupService_->createGroup(owner,groupName);
    if(!resultCreate.ok()){
        return makeRepoError(req,resultCreate.status,resultCreate.message);
    }
    if(!resultCreate.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"value invalid");
    }
    session.joinedGroupIds_.insert(resultCreate.value.value().groupId);
    return makeOk(req,im::MsgType::CREATE_GROUP_RESP,nlohmann::json{{"groupId",resultCreate.value.value().groupId},{"groupName",groupName},{"ownerAccountId",owner},{"ownerUsername",session.username_}});
}
im::Imservice::BroadcastResult im::Imservice::broadcastToGroup(const std::string& groupId,ConnKey senderkey,im::Response& push){
    BroadcastResult result;
    push.req_id=0;//群推送消息不需要req_id，由服务器生成唯一msg_id
    std::optional<uint64_t> msgId=std::nullopt;
    if(push.data.contains("msgId")){
        msgId=push.data["msgId"].get<uint64_t>();
    }
    decorate(push,msgId);//群推送消息也需要decorate添加msg_id和server_ts_ms等字段
    auto payload=encodeResponse(push);
    const auto& users=groupManager_.memberInfos(groupId);//根据群id取成员用户名列表
    for(const auto& user:users){
        const auto& keys=sessionManager_.connKeysByAccountId(user.accountId);
        for(const auto& key:keys){
            if(key!=senderkey){
                SendResult res=sendPush(key,payload);
                switch(res){
                    case SendResult::Ok:
                        result.sent++;
                        break;
                    case SendResult::NoSuchConnection:
                        result.noSuchConnection++;
                        break;
                    case SendResult::Closed:
                        result.closed++;
                        break;
                    case SendResult::Overloaded:
                        result.overloaded++;
                        break;
                }
            }
        }
    }
    return result;
   
}

im::Response im::Imservice::handleJoin(const im::Request & req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);//登录门禁
    if(err.has_value()){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    
    auto joinResult=groupManager_.joinGroup(groupId,session.accountId_);
    if(joinResult==JoinResult::ERR_NO_SUCH_GROUP){
        return makeErr(req,im::ErrorCode::NO_SUCH_GROUP,"no such group");
    }
    if(joinResult==JoinResult::OK_ALREADY_IN){
        session.joinedGroupIds_.insert(groupId);//虽然已经在群里了，但为了防止session状态不一致，还是把群id加入session的joinedGroupIds_里
        return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"joined",false},{"alreadyIn",true}});
    }
    session.joinedGroupIds_.insert(groupId);
    if(hasRepositories()){
        auto result=repos_.groupRepo->addMember(groupId,session.accountId_,0);
        if(result.status!=storage::RepoStatus::Ok&&result.status!=storage::RepoStatus::AlreadyExists){

            groupManager_.leaveGroup(groupId,session.accountId_);//回滚内存状态
            session.joinedGroupIds_.erase(groupId);
            return makeRepoError(req,result.status,"filed to persist group member");
        }
    }
    return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"joined",true},{"alreadyIn",false}});
}

im::Response im::Imservice::handleLeave(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    if(groupManager_.isOwner(groupId,session.accountId_)){
        return makeErr(req,ErrorCode::OWNER_CANNOT_LEAVE,"owner can not leave the group");
    }
    QuitResult quitResult=groupManager_.leaveGroup(groupId,session.accountId_);
    if(quitResult==QuitResult::ERR_NO_SUCH_GROUP){
        return makeErr(req,im::ErrorCode::NO_SUCH_GROUP,"No such group");
    }
    if(quitResult==QuitResult::ERR_NOT_IN_GROUP){
        session.joinedGroupIds_.erase(groupId);//虽然不在群里了，但为了防止session状态不一致，还是把群id从session的joinedGroupIds_里移除掉
        return makeOk(req,im::MsgType::LEAVE_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"left",false},{"alreadyLeft",true}});
    }
    session.joinedGroupIds_.erase(groupId);
    if (quitResult == QuitResult::OK_LEFT) {
    if (hasRepositories()) {
        auto result = repos_.groupRepo->removeMember(groupId, session.accountId_);

        if (result.status != storage::RepoStatus::Ok &&
            result.status != storage::RepoStatus::NotFound) {
            groupManager_.joinGroup(groupId, session.accountId_);//回滚内存状态
            session.joinedGroupIds_.insert(groupId);
            return makeRepoError(req, result.status, "Failed to remove group member");
        }
    }
}

    return makeOk(req,im::MsgType::LEAVE_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"left",true},{"alreadyLeft",false}});
}
im::Response im::Imservice::handleGroupMsg(const im::Request &req ,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);//校验登录
    if(err.has_value()){
        return err.value();
    }
    //发消息限流
    if(rateLimiter_){
        auto limitResult=rateLimiter_->checkSendMessage(session.accountId_,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }    
    }
    
    if(imConfig_.requireGroupIdForSend){//如果配置要求必须提供groupId字段
        if(!req.body.contains("groupId")||!req.body["groupId"].is_string()){
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"groupId can not be empty");
        }
    }
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    auto errInGroup=guardInGroup(req,session,groupId);
    if(errInGroup.has_value()){
        return errInGroup.value();
    }
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    if(content.size()>imConfig_.maxMessageLen){
        return makeErr(req,im::ErrorCode::BAD_REQUEST,"Message content is too long");
    }
    if(!groupManager_.isMember(groupId,session.accountId_)){
        return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"The user is not in the group");
    }
    uint64_t serverTsMs=nowMs();
    uint64_t msgId=nextMessageId();
    if(hasRepositories()){//保存消息
        auto result=repos_.messageRepo->saveGroupMessage(msgId,groupId,session.accountId_,session.username_,content,serverTsMs);
        if(!result.ok()){
            return makeRepoError(req,result.status,"failed to save group message");
        }
    }
    //更新群聊会话列表
    if(conversationService_){
        auto memberInfos=groupManager_.memberInfos(groupId);
        std::vector<std::string> memberAccountIds;
        memberAccountIds.reserve(memberInfos.size());
        for(const auto& memberInfo:memberInfos){
            memberAccountIds.push_back(memberInfo.accountId);
        }
        auto conversationResult=conversationService_->recordGroupMessage(groupId,memberAccountIds,session.accountId_,session.username_,msgId,content,serverTsMs);
        if(!conversationResult.ok()){
            LOG_WARN("Failed to update group conversation");
        }
    }
    //广播在线成员
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::GROUP_MSG_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New room message",.data=nlohmann::json{{"fromAccountId",session.accountId_},{"fromUsername",session.username_},{"groupId",groupId},{"content",content},{"msgId",msgId}}};
    BroadcastResult result=broadcastToGroup(groupId,key,pushMsg);
    saveOfflineForGroupMembers(groupId,session.accountId_,msgId);
    return makeOk(req,im::MsgType::GROUP_MSG_RESP,nlohmann::json{{"groupId",groupId},{"sent",result.sent},{"dropped",result.dropped()},{"noSuchConnection",result.noSuchConnection},{"closed",result.closed},{"overloaded",result.overloaded}});

}
im::Response im::Imservice::handleGroupMembers(const im::Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(imConfig_.requireGroupIdForSend){
        if(!req.body.contains("groupId")||!req.body["groupId"].is_string()){
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"groupId can not be empty");
        }
    }
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    auto errInGroup=guardInGroup(req,session,groupId);
    if(errInGroup.has_value()){
        return errInGroup.value();
    }
    //同步查成员账号资料

    auto membersJson=buildMemberProfileList(groupId);
        return makeOk(req,im::MsgType::GROUP_MEMBERS_RESP,nlohmann::json{{"groupId",groupId},{"members",membersJson}});
}

im::Response im::Imservice::handleKickGroupMember(const Request& req, [[maybe_unused]]ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取目标账号
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    //禁止自己踢自己
    if(targetAccountId==session.accountId_){
        return makeErr(req,ErrorCode::CANNOT_KICK_SELF,"You cannot kick yourself");
    }
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }
    auto resultKick=groupService_->kickMember(groupId,session.accountId_,targetAccountId);
    if(!resultKick.ok()){
        return makeRepoError(req,resultKick.status,resultKick.message);
    }
    
    //同步删除在线session中的群聊
    sessionManager_.removeJoinedGroup(targetAccountId,groupId);
    //给被踢用户推送被踢事件
    im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="you have been kicked from the group",.data=nlohmann::json{{"event","be kicked"},{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId}}};
    auto result=pushToAccount(targetAccountId,pushEvent);
    if(result.sent==0){
        LOG_WARN("Failed to push the event to the accountId:"+targetAccountId);
    }
    //给群内其他成员广播成员被踢出事件
    im::Response groupPushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg=targetAccountId+" have been kicked from the group",.data=nlohmann::json{{"event","member_removed"},{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId}}};
    auto broastResult=broadcastToGroup(groupId,key,groupPushEvent);
    return makeOk(req,MsgType::KICK_GROUP_MEMBER_RESP,nlohmann::json{{"groupId",groupId},{"targetAccountId",targetAccountId},{"removed",true},{"sent",broastResult.sent},{"closed",broastResult.closed},{"noSuchConnection",broastResult.noSuchConnection}});
}
im::Response im::Imservice::handleSetGroupAdmin(const Request& req, [[maybe_unused]]ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取目标账号
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    //读取enable
    bool enable = false;

    if(!req.body.contains("enable") ||!req.body["enable"].is_boolean())
    {
        return makeErr(req,ErrorCode::BAD_REQUEST,"invalid enable");
    }
    enable =req.body["enable"].get<bool>();
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }

    auto result=groupService_->setAdmin(groupId,session.accountId_,targetAccountId,enable);
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    //群内广播成员管理员变更
    im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="group admin changed",.data=nlohmann::json{{"event","group admin changed"},{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId},{"enable",enable}}};
    
    auto broastResult=broadcastToGroup(groupId,key,pushEvent);
    return makeOk(req,MsgType::SET_GROUP_ADMIN_RESP,nlohmann::json{{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId},{"enable",enable},{"sent",broastResult.sent},{"closed",broastResult.closed},{"noSuchConnection",broastResult.noSuchConnection}});
}
im::Response im::Imservice::handleTransferGroupOwner(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取目标账号
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }

    auto result=groupService_->transferOwner(groupId,session.accountId_,targetAccountId);
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    //群内推送群主转让事件
    im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="group owner changed",.data=nlohmann::json{{"event","group owner changed"},{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId}}};
    
    auto broastResult=broadcastToGroup(groupId,key,pushEvent);
    return makeOk(req,MsgType::TRANSFER_GROUP_OWNER_RESP,nlohmann::json{{"groupId",groupId},{"oldOwner",session.accountId_},{"newOwner",targetAccountId},{"sent",broastResult.sent},{"closed",broastResult.closed},{"noSuchConnection",broastResult.noSuchConnection}});
}

im::Response im::Imservice::handleInviteGroupMember(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取目标账号
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }

    auto result=groupService_->inviteMember(groupId,session.accountId_,targetAccountId);
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"groupInviteResult value is empty");
    }
    if(result.value.value().joined){
        sessionManager_.addJoinedGroup(targetAccountId,groupId);
        //群内邀请成员事件广播
        im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="member_invired",.data=nlohmann::json{{"event","member_invited"},{"groupId",groupId},{"operatorAccountId",session.accountId_},{"targetAccountId",targetAccountId}}};
        broadcastToGroup(groupId,key,pushEvent);
    }
    if(result.value.value().alreadyIn){
        return makeOk(req,MsgType::INVITE_GROUP_MEMBER_RESP,nlohmann::json{{"groupId",groupId},{"targetAccountId",targetAccountId},{"joined",false},{"alreadyIn",true}});
    }
    return makeOk(req,MsgType::INVITE_GROUP_MEMBER_RESP,nlohmann::json{{"groupId",groupId},{"targetAccountId",targetAccountId},{"joined",true},{"alreadyIn",false}});
}
im::Response im::Imservice::handleDissolveGroup(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }

    auto result=groupService_->dissolveGroup(groupId,session.accountId_,nowMs());
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"groupDissolveResult value is empty");
    }
    if(result.value.value().alreadyDissolved){
        return makeOk(req,MsgType::DISSOLVE_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"dissolved",false},{"alreadyDissolved",true}});
    }
    auto accountIds=result.value.value().affectedAccountIds;
    im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="group dissolved",.data=nlohmann::json{{"event","group_dissolved"},{"groupId",groupId},{"operatorAccountId",session.accountId_}}};
    //广播群解散事件
    for(const auto& accountId:accountIds){
        pushToAccount(accountId,pushEvent);
    }
    //同步在线状态
    sessionManager_.removeJoinedGroupForAccounts(accountIds,groupId);
    return makeOk(req,MsgType::DISSOLVE_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"operatorAccountId",session.accountId_},{"dissolved",true},{"alreadyDissolved",false},{"affectedMembers",accountIds.size()}});
}
im::Response im::Imservice::handleApplyGroupJoin(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取申请信息
    std::string message;
    auto getMsg=getStringField(req,"message",message);
    if(getMsg){
        return getMsg.value();
    }
    if(!groupJoinService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupJoinService is not avaiable");
    }
    auto result=groupJoinService_->applyToJoin(groupId,session.accountId_,message,nowMs());
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"value is empty");
    }
    auto applyRes=result.value.value();
    return makeOk(req,MsgType::APPLY_GROUP_JOIN_RESP,nlohmann::json{{"groupId",groupId},{"applicantAccountId",session.accountId_},{"submitted",applyRes.submitted},{"alreadyPending",applyRes.alreadyPending},{"alreadyIn",applyRes.alreadyIn}});
}
im::Response im::Imservice::handleListGroupJoinRequest(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取limit
    auto limit=parseLimit(req,"limit",20,100);
    if(!groupJoinService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupJoinService is not avaiable");
    }

    auto result=groupJoinService_->listPendingRequests(groupId,session.accountId_,limit);
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"value is empty");
    }
    auto requestRecords=result.value.value();
    nlohmann::json recordJsons=nlohmann::json::array();
    for(const auto& record:requestRecords){
        recordJsons.emplace_back(nlohmann::json{{"applicant_account_id",record.applicantAccountId},
        {"request_id",record.requestId},{"group_id",record.groupId},
        {"reviewer_account_id",record.reviewerAccountId},{"message",record.requestMessage},
        {"status",record.status},{"created_at_ms",record.createdAtMs},
        {"reviewed_at_ms",record.reviewedAtMs}});
    }
    return makeOk(req,MsgType::LIST_GROUP_JOIN_REQUESTS_RESP,nlohmann::json{{"groupId",groupId},{"reviewerAccountId",session.accountId_},{"requestRecord",recordJsons}});
}
im::Response im::Imservice::handleSearchGroups(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    
    if(!groupService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }
    auto result=groupService_->searchGroupById(groupId,session.accountId_);
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"value is empty");
    }
    auto groupRes=result.value.value();
    return makeOk(req,MsgType::SEARCH_PUBLIC_GROUPS_RESP,nlohmann::json{{"groupId",groupId},{"requesterAccountId",session.accountId_},{"groupName",groupRes.groupName},{"ownerAccountId",groupRes.ownerAccountId},{"memberCount",groupRes.memberCount},{"alreadyMember",groupRes.alreadyMember}});
}



im::Response im::Imservice::handleReviewGroupJoin(const Request& req,[[maybe_unused]] ConnKey key, Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    auto getGroupId=getStringField(req,"groupId",groupId);
    if(getGroupId){
        return getGroupId.value();
    }
    //读取申请人账号
    std::string applicantAccountId;
    auto getApplicant=getStringField(req,"applicantAccountId",applicantAccountId);
    if(getApplicant){
        return getApplicant.value();
    }
    //读取是否同意
    bool approve=false;
    if(!req.body.contains("approve")||!req.body["approve"].is_boolean()){
        return makeErr(req,ErrorCode::MISSING_FIELD,"missing field approve ");
    }
    approve=req.body["approve"].get<bool>();
    if(!groupJoinService_){
        return makeErr(req,ErrorCode::INTERNAL,"groupService is not avaiable");
    }
    auto result=groupJoinService_->reviewRequest(groupId,applicantAccountId,session.accountId_,approve,nowMs());
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"value is empty");
    }
    auto reviewRes=result.value.value();
        if(!reviewRes.alreadyHandled){
        //向申请人推送
        im::Response pushEvent{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="join request approve",.data=nlohmann::json{{"event","join_request"},{"groupId",groupId},{"reviewerAccountId",session.accountId_},{"applicantAccountId",applicantAccountId},{"approved",reviewRes.approved},{"rejected",reviewRes.rejected},{"memberAdded",reviewRes.memberAdded}}};
        pushToAccount(applicantAccountId,pushEvent);
        if(reviewRes.memberAdded){//成功入群同步状态
            sessionManager_.addJoinedGroup(applicantAccountId,groupId);
            //向申请人推送
            im::Response push{.ver=1,.req_id=0,.type=im::MsgType::GROUP_EVENT_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="member joined",.data=nlohmann::json{{"event","member_joined"},{"groupId",groupId},{"reviewerAccountId",session.accountId_},{"applicantAccountId",applicantAccountId}}};
            broadcastToGroup(groupId,key,push);
        }
    }
    return makeOk(req,MsgType::REVIEW_GROUP_JOIN_REQUEST_RESP,nlohmann::json{{"groupId",groupId},{"requesterAccountId",session.accountId_},{"applicantAccountId",applicantAccountId},{"approved",reviewRes.approved},{"rejected",reviewRes.rejected},{"memberAdded",reviewRes.memberAdded},{"alreadyHandled",reviewRes.alreadyHandled}});
}



im::Response im::Imservice::handleListGroups(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::vector<std::string> groups=groupManager_.groupsOfUser(session.accountId_);
    return makeOk(req,MsgType::LIST_GROUPS_RESP,nlohmann::json{{"groupIds",groups},{"count",groups.size()}});
}


LogContext im::Imservice::makeReqCtx(ConnKey key,const Request& req,const Session& session,const std::string& event)const{
    LogContext ctx;
    ctx.connFd=static_cast<int>(key);
    ctx.event=event;
    ctx.reqId=static_cast<uint64_t>(req.req_id);
    ctx.msgType=static_cast<uint32_t>(im::msgTypeToInt(req.type));
    if(!session.username_.empty()){
        ctx.user=session.username_;
    }
    else if(!req.from.empty()){
        ctx.user=req.from;
    }
    ctx.groupId=tryExtractGroupId(req);
    return ctx;
}
LogContext im::Imservice::makeRespCtx(ConnKey key,const Request& req,const Response& resp,const Session& session,const std::string& event)const{
    LogContext ctx;
    ctx.connFd=static_cast<int>(key);
    ctx.event=event;
    ctx.reqId=static_cast<uint64_t>(req.req_id);
    ctx.msgType=static_cast<uint32_t>(im::msgTypeToInt(resp.type));
    ctx.errCode=static_cast<uint32_t>(resp.code);
    ctx.msgId=tryExtractMsgId(resp);
    if(!session.username_.empty()){
        ctx.user=session.username_;
    }
    else if(!req.from.empty()){
        ctx.user=req.from;
    }
    if(resp.data.contains("groupId")&&resp.data["groupId"].is_string()){
        ctx.groupId=resp.data["groupId"];
    }
    else{
        ctx.groupId=tryExtractGroupId(req);
    }
    if(resp.data.contains("sent")&&resp.data["sent"].is_number_unsigned()){
        ctx.sent=resp.data["sent"].get<size_t>();
    }
    if(resp.data.contains("dropped")&&resp.data["dropped"].is_number_unsigned()){
        ctx.dropped=resp.data["dropped"].get<size_t>();
    }
    if(resp.data.contains("noSuchConnection")&&resp.data["noSuchConnection"].is_number_unsigned()){
        ctx.noSuchConnection=resp.data["noSuchConnection"].get<size_t>();
    }
    if(resp.data.contains("closed")&&resp.data["closed"].is_number_unsigned()){
        ctx.closed=resp.data["closed"].get<size_t>();
    }
    if(resp.data.contains("overloaded")&&resp.data["overloaded"].is_number_unsigned()){
        ctx.overloaded=resp.data["overloaded"].get<size_t>();
    }
    if(resp.data.contains("pendingBytes")&&resp.data["pendingBytes"].is_number_unsigned()){
        ctx.pendingBytes=resp.data["pendingBytes"].get<size_t>();
    }
    return ctx;
}
std::optional<std::string> im::Imservice::tryExtractGroupId(const Request& req)const{
    if(req.body.contains("groupId")&&req.body["groupId"].is_string()){
        std::string groupId=req.body["groupId"];
        if(!groupId.empty())
            return groupId;
    }
    return std::nullopt;
}
std::optional<uint64_t> im::Imservice::tryExtractMsgId(const Response& resp)const{
    if(resp.data.contains("msg_id")){
        if(resp.data["msg_id"].is_number_unsigned()){
            return resp.data["msg_id"].get<uint64_t>();
        }
        if(resp.data["msg_id"].is_number_integer()&&resp.data["msg_id"].get<int64_t>()>=0){
            return static_cast<uint64_t>(resp.data["msg_id"].get<int64_t>());
        }
    }
    return std::nullopt;
}

//统一错误处理
LogLevel im::Imservice::mapErrorToLogLevel(im::ErrorCode code) const{
    switch(code){
        case im::ErrorCode::BAD_JSON:
        case im::ErrorCode::MISSING_FIELD:
        case im::ErrorCode::UNSUPPORTED_VER:
        case im::ErrorCode::UNKNOWN_TYPE:
        case im::ErrorCode::BAD_REQUEST:
        case im::ErrorCode::GROUP_NAME_INVALID:
        case im::ErrorCode::NO_SUCH_USER:
        case im::ErrorCode::NO_SUCH_GROUP:
        case im::ErrorCode::ALREADY_IN_GROUP:
        case im::ErrorCode::NOT_IN_GROUP:
        case im::ErrorCode::NOT_AUTHED:
        case im::ErrorCode::USER_NOT_FOUND:
        case im::ErrorCode::BAD_PASSWORD:
        case im::ErrorCode::WEAK_PASSWORD:
        case im::ErrorCode::USER_EXISTS:
        case im::ErrorCode::TOKEN_INVALID:
        case im::ErrorCode::TOKEN_EXPIRED:
        case im::ErrorCode::TOKEN_REVOKED:
        case im::ErrorCode::PROFILE_NOT_FOUND:
        case im::ErrorCode::AVATAR_URL_TOO_LONG:
        case im::ErrorCode::SIGNATURE_TOO_LONG:
        case im::ErrorCode::NICKNAME_INVALID:
        case im::ErrorCode::CANNOT_ADD_SELF:
        case im::ErrorCode::ALREADY_FRIENDS:
        case im::ErrorCode::FRIEND_REQUEST_EXISTS:
        case im::ErrorCode::FRIEND_REQUEST_NOT_FOUND:
        case im::ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED:
        case im::ErrorCode::FRIEND_REQUEST_FORBIDDEN:
        case im::ErrorCode::INVALID_ACK_PAYLOAD:
        case im::ErrorCode::ACK_BATCH_TOO_LARGE:
        case im::ErrorCode::MESSAGE_NOT_FOUND:
        case im::ErrorCode::MESSAGE_ACK_FORBIDDEN:
        case im::ErrorCode::NO_PERMISSION:
        case im::ErrorCode::TARGET_NOT_IN_GROUP:
        case im::ErrorCode::OWNER_CANNOT_LEAVE:
        case im::ErrorCode::OWNER_CANNOT_BE_KICKED:
        case im::ErrorCode::INVALID_GROUP_ROLE:
        case im::ErrorCode::CANNOT_KICK_SELF:
        case im::ErrorCode::GROUP_DISSOLVED:
        case im::ErrorCode::CANNOT_INVITE_SELF:
        case im::ErrorCode::INVITE_REQUIRES_FRIEND:
        case im::ErrorCode::GROUP_MEMBER_LIMIT_REACHED:
        case im::ErrorCode::JOIN_REQUEST_NOT_FOUND:
            return LogLevel::WARN;
        case im::ErrorCode::INTERNAL:
            return LogLevel::ERROR;
        default:
            return LogLevel::ERROR;
    }
}
im::Imservice::SendResult im::Imservice::sendResponseWithLog(ConnKey key,const Request& req,Response& resp,const Session& session,const std::string& outEvet){
    decorate(resp,std::nullopt,req.req_id);
    auto payload=im::encodeResponse(resp);
    SendResult result;
    if(sendToConnKey_){
        result=sendToConnKey_(key,payload);
    }
    else{
        result=SendResult::NoSuchConnection;
    }
    auto ctx=makeRespCtx(key,req,resp,session,outEvet);
    ctx.sendResult=std::string(sendResultToString(result));
    if(result!=SendResult::Ok){//发送失败日志
        if(result==SendResult::NoSuchConnection){
            LOG_ERROR_CTX("Failed to send response, no such connection",ctx);
        }
        else if(result==SendResult::Closed){
            LOG_ERROR_CTX("Failed to send response, connection closed",ctx);
        }
        else if(result==SendResult::Overloaded){
            LOG_ERROR_CTX("Failed to send response, connection overloaded",ctx);
        }
        return result;
    }

    if(!resp.ok){//响应本身表示处理请求失败，记录错误日志
        LogLevel level=mapErrorToLogLevel(resp.code);
        if(level==LogLevel::ERROR){
            LOG_ERROR_CTX("im response error",ctx);
        }
        else if(level==LogLevel::WARN){
            LOG_WARN_CTX("im response warn",ctx);
        }
    }
    else{
        LOG_INFO_CTX("im response success",ctx);
    }
    return result;
}
im::Imservice::SendResult im::Imservice::sendParseErrorWithLog(ConnKey key,Response& resp,const Session& session){
    decorate(resp);
    auto payload=im::encodeResponse(resp);
    SendResult result;
    if(sendToConnKey_){
        result=sendToConnKey_(key,payload);
    }
    else{
        result=SendResult::NoSuchConnection;
    }
    auto ctx=makeRespCtx(key,Request{},resp,session,"PARSE_ERR_RESP");
    LogLevel level=mapErrorToLogLevel(resp.code);
    if(level==LogLevel::ERROR){
        LOG_ERROR_CTX("im parse error",ctx);
    }
    else if(level==LogLevel::WARN){
        LOG_WARN_CTX("im parse warn",ctx);
    }
    if(result!=SendResult::Ok){
        LOG_ERROR_CTX("Failed to send parse error response",ctx);
    }
    return result;
}
im::Response im::Imservice::dispatcResqest(const Request& req,ConnKey key,Session& session){
    switch(req.type){
        case im::MsgType::AUTH_REQ:
            return handleAuth(req,key,session);
        case im::MsgType::ECHO_REQ:
            return handleEcho(req,key,session);
        case im::MsgType::DM_REQ:
            return handleDm(req,key,session);
        case im::MsgType::LIST_USERS_REQ:
            return handleListUsers(req,key,session);
        case im::MsgType::CREATE_GROUP_REQ:
            return handleCreateGroup(req,key,session);
        case im::MsgType::JOIN_GROUP_REQ:
            return handleApplyGroupJoin(req,key,session);
        case im::MsgType::LEAVE_GROUP_REQ:
        {
            im::Response resp=handleLeave(req,key,session);
            if(resp.ok&&resp.data.contains("left")&&resp.data["left"].get<bool>()==true){
                    std::string groupId=resp.data["groupId"];
                    LOG_INFO_CTX("im leave group",makeRespCtx(key,req,resp,session,"LEAVE_GROUP"));
                    im::Response leaveEvent=makeOk(req,im::MsgType::GROUP_EVENT_PUSH,nlohmann::json{{"event","leave"},{"accountId",session.accountId_},{"username",session.username_},{"groupId",groupId}});
                    broadcastToGroup(groupId,key,leaveEvent);
                }
            return resp;
        }
        case im::MsgType::GROUP_MSG_REQ:
            return handleGroupMsg(req,key,session);
        case im::MsgType::GROUP_MEMBERS_REQ:
            return handleGroupMembers(req,key,session);
        case im::MsgType::LIST_GROUPS_REQ:
            return handleListGroups(req,key,session);
        case im::MsgType::GROUP_HISTORY_REQ:
            return handleGroupHistory(req,key,session);
        case im::MsgType::OFFLINE_LIST_REQ:
            return handleOfflinelist(req,key,session);
        case im::MsgType::OFFLINE_ACK_REQ:
            return handleOfflineAck(req,key,session);
        case im::MsgType::REGISTER_REQ:
            return handleRegister(req,key,session);
        case im::MsgType::LOGIN_REQ:
            return handleLogin(req,key,session);
        case im::MsgType::LOGOUT_REQ:
            return handleLogout(req,key,session);
        case im::MsgType::TOKEN_LOGIN_REQ:
            return handleTokenLogin(req,key,session);
        case im::MsgType::GET_PROFILE_REQ:
            return handleGetProfile(req,key,session);
        case im::MsgType::UPDATE_PROFILE_REQ:
            return handleUpdateProfile(req,key,session);
        case im::MsgType::SEARCH_USER_REQ:
            return handleSearchUser(req,key,session);
        case im::MsgType::LIST_FRIENDS_REQ:
            return handleListFriends(req,key,session);
        case im::MsgType::SEND_FRIEND_REQUEST_REQ:
            return handleSendFriendRequest(req,key,session);
        case im::MsgType::LIST_FRIEND_REQUEST_REQ:
            return handleListFriendRequests(req,key,session);
        case im::MsgType::ACCEPT_FRIEND_REQUEST_REQ:
            return handleAcceptFriendRequest(req,key,session);
        case im::MsgType::REJECT_FRIEND_REQUEST_REQ:
            return handleRejectFriendRequest(req,key,session);
        case im::MsgType::REMOVE_FRIEND_REQ:
            return handleRemoveFriend(req,key,session);
        case im::MsgType::DM_HISTORY_REQ:
            return handleDmHistory(req,key,session);
        case im::MsgType::CONVERSATION_LIST_REQ:
            return handleConversationList(req,key,session);
        case im::MsgType::CONVERSATION_READ_REQ:
            return handleConversationRead(req,key,session);
        case im::MsgType::SYNC_REQ:{
            LOG_INFO_CTX("sync request in",makeReqCtx(key,req,session,"SYNC_IN"));
            return handleSync(req,key,session);
        }
        case im::MsgType::MESSAGE_ACK_REQ:
            return handleMessageAck(req,key,session);
        case im::MsgType::KICK_GROUP_MEMBER_REQ:
            return handleKickGroupMember(req,key,session);
        case im::MsgType::SET_GROUP_ADMIN_REQ:
            return handleSetGroupAdmin(req,key,session);
        case im::MsgType::TRANSFER_GROUP_OWNER_REQ:
            return handleTransferGroupOwner(req,key,session);
        case im::MsgType::INVITE_GROUP_MEMBER_REQ:
            return handleInviteGroupMember(req,key,session);
        case im::MsgType::DISSOLVE_GROUP_REQ:
            return handleDissolveGroup(req,key,session);
        case im::MsgType::APPLY_GROUP_JOIN_REQ:
            return handleApplyGroupJoin(req,key,session);
        case im::MsgType::LIST_GROUP_JOIN_REQUESTS_REQ:
            return handleListGroupJoinRequest(req,key,session);
        case im::MsgType::REVIEW_GROUP_JOIN_REQUEST_REQ:
            return handleReviewGroupJoin(req,key,session);
        case im::MsgType::SEARCH_PUBLIC_GROUPS_REQ:
            return handleSearchGroups(req,key,session);
        default:
            return makeErr(req,im::ErrorCode::UNKNOWN_TYPE,"Unknown message type");
    }
}


//存储接口
im::ErrorCode im::Imservice::repoStatusToErrorCode(storage::RepoStatus status)const{
    switch(status){
        case storage::RepoStatus::Ok:
            return im::ErrorCode::OK;
        case storage::RepoStatus::AlreadyExists:
            return im::ErrorCode::USER_EXISTS;
        case storage::RepoStatus::InvalidArgument:
            return im::ErrorCode::BAD_REQUEST;
        case storage::RepoStatus::SqlError:
            return im::ErrorCode::INTERNAL;
        case storage::RepoStatus::NotFound:
            return im::ErrorCode::NO_SUCH_GROUP;
        case storage::RepoStatus::CannotAddYourself:
            return im::ErrorCode::CANNOT_ADD_SELF;
        case storage::RepoStatus::AlreadyFriends:
            return im::ErrorCode::ALREADY_FRIENDS;
        case storage::RepoStatus::AlreadyHandled:
            return im::ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED;
        case storage::RepoStatus::Forbidden:
            return im::ErrorCode::FRIEND_REQUEST_FORBIDDEN;
        case storage::RepoStatus::NotFriends:
            return im::ErrorCode::NOT_FRIENDS;
        case storage::RepoStatus::NoPermission:
            return im::ErrorCode::NO_PERMISSION;
        case storage::RepoStatus::TargetNotInGroup:
            return im::ErrorCode::TARGET_NOT_IN_GROUP;
        case storage::RepoStatus::OwnerCannotLeave:
            return im::ErrorCode::OWNER_CANNOT_LEAVE;
        case storage::RepoStatus::OwnerCannotBeKicked:
            return im::ErrorCode::OWNER_CANNOT_BE_KICKED;
        case storage::RepoStatus::InvalidGroupRole:
            return im::ErrorCode::INVALID_GROUP_ROLE;
        case storage::RepoStatus::GroupDissolved:
            return im::ErrorCode::GROUP_DISSOLVED;
        case storage::RepoStatus::CannotInivteSelf:
            return im::ErrorCode::CANNOT_INVITE_SELF;
        case storage::RepoStatus::InviteRequestsFriend:
            return im::ErrorCode::INVITE_REQUIRES_FRIEND;
        case storage::RepoStatus::GroupMemberLimitReach:
            return im::ErrorCode::GROUP_MEMBER_LIMIT_REACHED;
        case storage::RepoStatus::UserNotFound:
            return im::ErrorCode::NO_SUCH_USER;
        case storage::RepoStatus::JoinRequestNotFound:
            return im::ErrorCode::JOIN_REQUEST_NOT_FOUND;
        case storage::RepoStatus::GroupNotFound:
            return im::ErrorCode::NO_SUCH_GROUP;
        case storage::RepoStatus::Internal:
            return im::ErrorCode::INTERNAL;
    }
    return im::ErrorCode::INTERNAL;
}
im::Response im::Imservice::makeRepoError(const im::Request& req,storage::RepoStatus status,const std::string& fallbackMsg) const{
    auto code=repoStatusToErrorCode(status);
    std::string msg=fallbackMsg;
    if(status==storage::RepoStatus::SqlError){
        msg="Storage internal error";
    }
    return makeErr(req,code,msg);
}
im::Response im::Imservice::handleGroupHistory(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //历史消息限流
    if(rateLimiter_){
        auto limitResult=rateLimiter_->checkHistory(session.accountId_,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }
    }
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    
    auto accountId=sessionManager_.accountIdByConn(key);
    if(!accountId){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"User is not exist");
    }
    if(!groupManager_.isMember(groupId,accountId.value())){
        return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"The user is not in the group");
    }
    if(!hasRepositories()||!repos_.messageRepo){
        return makeErr(req,im::ErrorCode::INTERNAL,"Message repository is not configured");
    }
    auto historyQuery=parseHistoryQuery(req,imConfig_.defaultHistoryLimit,imConfig_.maxHistoryLimit);
    if(!historyQuery.ok){
        return makeErr(req,historyQuery.code,historyQuery.message);
    }
    std::vector<storage::MessageRecord> messages;
    if(historyQuery.query.mode==HistoryQueryMode::After){
        messages=repos_.messageRepo->listGroupMessagesAfter(groupId,historyQuery.query.lastMsgId,historyQuery.query.limit);
    }
    else {
        messages=repos_.messageRepo->listGroupMessages(groupId,historyQuery.query.beforeMsgId,historyQuery.query.limit);
    }

    //返回JSON数组messages
    nlohmann::json messagesJson=nlohmann::json::array();
    for(const auto& msg:messages){
        messagesJson.push_back(nlohmann::json{{"msgId",msg.messageId},{"groupId",msg.groupId},{"senderAccountId",msg.senderAccountId},{"senderUsername",msg.senderUsername},{"content",msg.content},{"serverTsMs",msg.serverTsMs}});
    }
    return makeOk(req,im::MsgType::GROUP_HISTORY_RESP,nlohmann::json{{"groupId",groupId},{"mode",historyQueryModeToString(historyQuery.query.mode)},{"beforeMsgId",historyQuery.query.beforeMsgId},{"lastMsgId",historyQuery.query.lastMsgId},{"limit",historyQuery.query.limit},{"messages",messagesJson}});

}
im::Response im::Imservice::handleDmHistory(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    //历史消息限流
    if(rateLimiter_){
        auto limitResult=rateLimiter_->checkHistory(session.accountId_,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }
    }
    //读取目标
    std::string peerAccountId;
    auto getPeerAccountId=getStringField(req,"peerAccountId",peerAccountId);
    if(getPeerAccountId){
        return getPeerAccountId.value();
    }
    //查找账号
    if(!friendService_){
        return makeErr(req,im::ErrorCode::INTERNAL,"Friend service is not available");
    }
    auto userProfile=friendService_->findUser(peerAccountId);
    if(!userProfile){
        return makeErr(req,ErrorCode::NO_SUCH_USER,"no such user");
    }
    //确认是否好友关系
    if(!friendService_->areFriends(session.accountId_,peerAccountId)){
        return makeErr(req,ErrorCode::NOT_FRIENDS,"not your friends");
    }
    //生成会话key
    auto conversationKey=common::buildDirectConversationKey(session.accountId_,peerAccountId);
    //获取历史私聊消息
    if(!repos_.messageRepo){
        return makeErr(req,ErrorCode::INTERNAL,"messageRepo is not avaiable");
    }
    //解析beforeMsgId,limit
    auto historyQuery=parseHistoryQuery(req,imConfig_.defaultHistoryLimit,imConfig_.maxHistoryLimit);
    if(!historyQuery.ok){
        return makeErr(req,historyQuery.code,historyQuery.message);
    }
    //分支查询
    std::vector<storage::DirectMessageRecord> result;
    if(historyQuery.query.mode==HistoryQueryMode::After){
        result=repos_.messageRepo->listDirectMessagesAfter(conversationKey,historyQuery.query.lastMsgId,historyQuery.query.limit);
    }
    else{
        result=repos_.messageRepo->listDirectMessages(conversationKey,historyQuery.query.beforeMsgId,historyQuery.query.limit);
    }
    nlohmann::json messagesJson=nlohmann::json::array();
    for(const auto&message:result){
        messagesJson.push_back(nlohmann::json{{"msgId",message.messageId},{"fromAccountId",message.senderAccountId},{"toAccountId",message.receiverAccountId},{"fromUsername",message.senderUsername},{"content",message.content},{"serverTsMs",message.serverTsMs}});
    }
    return makeOk(req,MsgType::DM_HISTORY_RESP,nlohmann::json{{"peerAccountId",peerAccountId},{"conversationKey",conversationKey},{"mode",historyQuery.query.mode},{"beforeMsgId",historyQuery.query.beforeMsgId},{"lastMsgId",historyQuery.query.lastMsgId},{"limit",historyQuery.query.limit},{"messages",messagesJson}});
    
}
void im::Imservice::saveOfflineForGroupMembers(const std::string& groupId,const std::string& fromAccountId,uint64_t msgId){
    if(groupId.empty()||fromAccountId.empty()){
        return;
    }
    if(!repos_.offlineMessageRepo){
        return;
    }
    auto members=groupManager_.memberInfos(groupId);//获取群成员
    for(auto member:members){
        if(member.accountId!=fromAccountId){//跳过发送者
            auto keys=sessionManager_.connKeysByAccountId(member.accountId);
            if(keys.empty()){
                //用户各端都不在线
                auto result=repos_.offlineMessageRepo->saveOfflineMessage(member.accountId,msgId,groupId);
                if(!result.ok()){
                    LOG_WARN("Failed to save offlineMessage for"+member.accountId);
                }
            }
        }
    }

}
im::Response im::Imservice::handleOfflinelist(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);//校验已登录
    if(err){
        return err.value();
    }
    if(!repos_.offlineMessageRepo){
        //检查repos_离线存储是否存在
        return makeErr(req,ErrorCode::BAD_REQUEST,"OfflineMessageRepo is not exist");
    }
    //解析limit
    size_t limit=20;
    if(req.body.contains("limit")){
        
        if(req.body["limit"].is_number_unsigned()){
            limit=req.body["limit"].get<size_t>();
        }
        else if(req.body["limit"].is_number_integer()&&req.body["limit"].get<int64_t>()>0){
            limit=static_cast<size_t>(req.body["limit"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"Invalid limit");
        }
    }
    if(limit>200){
        limit=200;//限制limit最大值
    }
    auto indexes=repos_.offlineMessageRepo->listOfflineMessage(session.accountId_,limit);
    nlohmann::json indexJson=nlohmann::json::array();
    for(const auto& index:indexes){
        if(index.type==storage::OfflineMessageType::Group){
             indexJson.emplace_back(nlohmann::json{{"msgId",index.msgId},{"type",storage::offlineMessageTypeToString(index.type)},{"groupId",index.groupId}});
        }
        else if(index.type==storage::OfflineMessageType::Direct){
             indexJson.emplace_back(nlohmann::json{{"msgId",index.msgId},{"type",storage::offlineMessageTypeToString(index.type)},{"peerAccountId",index.peerAccountId}});
        }
    }
    return makeOk(req,MsgType::OFFLINE_LIST_RESP,nlohmann::json{{"messages",indexJson},{"count",indexJson.size()}});
}

im::Response im::Imservice::handleOfflineAck(const Request& req,[[maybe_unused]]ConnKey key,[[maybe_unused]]Session& session){
    //校验已经登录
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    if(!repos_.offlineMessageRepo){
        return makeErr(req,ErrorCode::BAD_REQUEST,"OfflineMessageRepo is not exist");
    }
    //解析msg_ids数组
    if(!req.body.contains("msg_ids")||!req.body["msg_ids"].is_array()){
        return makeErr(req,ErrorCode::BAD_JSON,"msg_ids Json is error");
    }
    const auto& msgIdsJsons=req.body.at("msg_ids");
    std::vector<uint64_t> msgIds;
    for(const auto& msgIdJson:msgIdsJsons){//过滤非法值
        if(msgIdJson.is_number_unsigned()){
            msgIds.push_back(msgIdJson.get<uint64_t>());
        }
        else if(msgIdJson.is_number_integer()&&msgIdJson.get<int64_t>()>0){
            msgIds.push_back(static_cast<uint64_t>(msgIdJson.get<int64_t>()));
        }
        else{
            return makeErr(req,ErrorCode::BAD_JSON,"Invalid msg_id in msg_ids");
        }
    }
    auto result=repos_.offlineMessageRepo->ackOfflineMessages(session.accountId_,msgIds);
    if(result.ok()){
        return makeOk(req,MsgType::OFFLINE_ACK_RESP,nlohmann::json{{"acked",msgIds.size()}});
    }
    return makeRepoError(req,result.status,result.message);
}


//登录注册接口

im::Response im::Imservice::handleRegister(const Request& req,[[maybe_unused]]ConnKey key,[[maybe_unused]]Session& session){
    //注册限流
    if(rateLimiter_){
        std::string ip = session.peerIp_.empty() ? "unknown" : session.peerIp_;
        auto limitResult=rateLimiter_->checkRegister(ip,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }
    }
    
    std::string username;
    auto getUsername=getStringField(req,"username",username);
    if(getUsername){
        return getUsername.value();
    }
    std::string password;
    auto getPassword=getStringField(req,"password",password);
    if(getPassword){
        return getPassword.value();
    }
    if(!authService_){
        return makeErr(req,ErrorCode::INTERNAL,"authService is not exist");
    }
    auto result=authService_->registerUser(username,password);
    if(result.ok){
        auto userInfo=result.user.value();
        return makeOk(req,MsgType::REGISTER_RESP,nlohmann::json{{"userId",userInfo.userId},{"accountId",userInfo.accountId},{"username",userInfo.username}});
    }
    if(result.status==auth::AuthStatus::AlreadyExist){
        return makeErr(req,ErrorCode::USER_EXISTS,"User is alreadyexist");
    }
    if(result.status==auth::AuthStatus::WeakPassword){
        return makeErr(req,ErrorCode::WEAK_PASSWORD,"Password is too weak");
    }
    return makeErr(req,ErrorCode::INTERNAL,"internal"+result.message);
}
im::Response im::Imservice::handleLogin(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    std::string accountId;
    auto getUsername=getStringField(req,"accountId",accountId);
    if(getUsername){
        return getUsername.value();
    }
    std::string password;
    auto getPassword=getStringField(req,"password",password);
    if(getPassword){
        return getPassword.value();
    }
    if(session.state_==ConnState::Authed){
        if(accountId==session.accountId_){
            return makeOk(req,MsgType::LOGIN_RESP);
        }
        else{
            return makeErr(req,ErrorCode::BAD_REQUEST,"username is different");
        }
    }
    if(!authService_){
        return makeErr(req,ErrorCode::INTERNAL,"authService is not exist");
    }
    auto result=authService_->login(accountId,password);
    if(!result.ok){
        if(result.status==auth::AuthStatus::UserNotFound){
            return makeErr(req,ErrorCode::USER_NOT_FOUND,"User is not exist");
        }
        if(result.status==auth::AuthStatus::BadPassword){
            //限制登录失败频率
            if(rateLimiter_){
                auto limitResult=rateLimiter_->checkLoginFail(accountId,nowMs());
                auto resultOpt=checkRateLimitOrError(req,limitResult);
                if(resultOpt){
                    return resultOpt.value();
                }
            }
            
            return makeErr(req,ErrorCode::BAD_PASSWORD,"Password is incorrect");
        }
        if(result.status==auth::AuthStatus::UserDisabled){
            return makeErr(req,ErrorCode::BAD_REQUEST,"User is disabled");
        }
        return makeErr(req,ErrorCode::INTERNAL,"Internal error");
    }
    //重置限制
    if (rateLimiter_) {
        rateLimiter_->resetLoginFail(accountId);
    }
    auto userInfo=result.user.value();
    if(!sessionManager_.bindUser(key,userInfo.userId,userInfo.accountId,userInfo.username)){
        return makeErr(req,ErrorCode::INTERNAL,"Failed to bind user");
    }
    session.userId_=result.user.value().userId;
    return makeOk(req,MsgType::LOGIN_RESP,nlohmann::json{{"userId",session.userId_},{"accountId",userInfo.accountId},{"username",userInfo.username},{"token",result.issuedToken.value().rawToken},{"expireAtMs",result.issuedToken.value().expireAtMs}});
}
im::Response im::Imservice::handleTokenLogin(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    std::string token;
    auto getToken=getStringField(req,"token",token);
    if(getToken){
        return getToken.value();
    }
    if(session.state_==ConnState::Authed){
        //当前session已经Authed，幂等处理
        return makeOk(req,MsgType::TOKEN_LOGIN_RESP);
    }
    if(!authService_){
        return makeErr(req,ErrorCode::INTERNAL,"authService is not exist");
    }
    auto result=authService_->loginByToken(token);
    if(!result.ok){
        if(result.status==auth::AuthStatus::InvalidToken){
            return makeErr(req,ErrorCode::TOKEN_INVALID,"token is invalid");
        }
        if(result.status==auth::AuthStatus::TokenExpired){
            return makeErr(req,ErrorCode::TOKEN_EXPIRED,"token is expired");
        }
        if(result.status==auth::AuthStatus::TokenRevoked){
            return makeErr(req,ErrorCode::TOKEN_REVOKED,"token is revoked");
        }
        return makeErr(req,ErrorCode::INTERNAL,"Internal error");
    }
    if(!result.user.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"Internal error, user info is missing in token login result");
    }
    if(!result.tokenExpireAtMs.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"Internal error, token expire time is missing in token login result");
    }
    auto userInfo=result.user.value();
    if(!sessionManager_.bindUser(key,userInfo.userId,userInfo.accountId,userInfo.username)){
        return makeErr(req,ErrorCode::INTERNAL,"Failed to bind user");
    }
    session.userId_=result.user.value().userId;
    return makeOk(req,MsgType::TOKEN_LOGIN_RESP,nlohmann::json{{"userId",session.userId_},{"accountId",userInfo.accountId},{"username",userInfo.username},{"expireAtMs",result.tokenExpireAtMs.value()}});
}
im::Response im::Imservice::handleLogout(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    std::string token;
    auto getToken=getStringField(req,"token",token);
    if(getToken){
        return getToken.value();
    }
    if(!authService_){
        return makeErr(req,ErrorCode::INTERNAL,"authService is not exist");
    }
    auto result=authService_->logout(token);
    if(!result.ok&&!result.alreadyLoggedOut){
        if(result.status==auth::AuthStatus::InvalidToken){
            return makeErr(req,ErrorCode::TOKEN_INVALID,"token is invalid");
        }
        if(result.status==auth::AuthStatus::TokenExpired){
            return makeErr(req,ErrorCode::TOKEN_EXPIRED,"token is expired");
        }
        return makeErr(req,ErrorCode::INTERNAL,"Internal error");
    }
    std::string username=session.username_;
    //注销
    if(result.ok||result.alreadyLoggedOut){
        //若当前连接已经绑定用户，则解绑
        if(session.state_==ConnState::Authed){
            sessionManager_.unbindConn(key);
            session.accountId_.clear();
            session.state_=ConnState::Connected;
            session.joinedGroupIds_.clear();
            session.userId_=0;
        }

        return makeOk(req,MsgType::LOGOUT_RESP,nlohmann::json{{"accountId",session.accountId_},{"revoked",result.revokedNow},{"alreadyRevoked",result.alreadyLoggedOut}});
    }
    return makeErr(req,ErrorCode::INTERNAL,"Internal error");
}

//用户资料
im::Response im::Imservice::handleGetProfile(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    if(!repos_.userProfileRepo){
        return makeErr(req,ErrorCode::INTERNAL,"userProfile is not exist");
    }
    auto result=repos_.userProfileRepo->findByUserId(session.userId_);
    if(result){
        auto userProfile=result.value();
        return makeOk(req,MsgType::GET_PROFILE_RESP,nlohmann::json{{"userId",userProfile.userId},{"accountId",userProfile.accountId},{"username",userProfile.username},{"nickname",userProfile.nickname},{"avatarUrl",userProfile.avatarUrl},{"signature",userProfile.signature},{"updateAtMs",userProfile.updateAtMs}});
    }
    
    return makeErr(req,ErrorCode::PROFILE_NOT_FOUND,"Failed to get profile");
}
im::Response im::Imservice::handleUpdateProfile(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取body
    std::string nickname;
    auto getNickname=getStringField(req,"nickname",nickname);
    if(getNickname){
        return getNickname.value();
    }
    if(nickname.empty()||nickname.size()>64){
        return makeErr(req,ErrorCode::NICKNAME_INVALID,"nickname invalid");
    }
    std::string avatarUrl;
    auto getUrl=getStringField(req,"avatarUrl",avatarUrl);
    if(getUrl){
        return getUrl.value();
    }
    if(avatarUrl.size()>512){
        return makeErr(req,ErrorCode::AVATAR_URL_TOO_LONG,"avatarUrl is too long");
    }
    std::string signature;
    auto getSignature=getStringField(req,"signature",signature);
    if(getSignature){
        return getSignature.value();
    }
    if(signature.size()>256){
        return makeErr(req,ErrorCode::SIGNATURE_TOO_LONG,"signature is too long");
    }
    if(!repos_.userProfileRepo){
        return makeErr(req,ErrorCode::INTERNAL,"userProfile is not exist");
    }
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto result=repos_.userProfileRepo->updateProfile(session.userId_,nickname,avatarUrl,signature,nowMs);
    if(result.ok()){
        auto profileResult=repos_.userProfileRepo->findByUserId(session.userId_);
        if(profileResult){
            auto userProfile=profileResult.value();
            return makeOk(req,MsgType::UPDATE_PROFILE_RESP,nlohmann::json{{"userId",userProfile.userId},{"username",userProfile.username},{"nickname",userProfile.nickname},{"avatarUrl",userProfile.avatarUrl},{"signature",userProfile.signature},{"updateAtMs",userProfile.updateAtMs}});
        }
        return makeErr(req,ErrorCode::PROFILE_NOT_FOUND,"Failed to get profile");
    }
    return makeErr(req,ErrorCode::INTERNAL,"Failed to update profile");
}

nlohmann::json im::Imservice::buildMemberProfileList(const std::string&groupId){
    storage::UserProfile emptyUserProfile;
    if(!groupService_){
        return nlohmann::json{{"accountId",emptyUserProfile.accountId},{"username",emptyUserProfile.username},{"nickname",emptyUserProfile.nickname},{"avatarUrl",emptyUserProfile.avatarUrl},{"signature",emptyUserProfile.signature}};
    }
    auto views=groupService_->listMemberViews(groupId);
    nlohmann::json profileList=nlohmann::json::array();
    for(const auto& view:views){
        profileList.push_back(nlohmann::json{{"accountId",view.accountId},{"username",view.username},{"nickname",view.nickname},{"avatarUrl",view.avatarUrl},{"role",roleToString(view.role)}});
    }
    return profileList;
}

//好友相关接口
im::Response im::Imservice::handleSearchUser(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //读取账号
    std::string accountId;
    auto getAccountId=getStringField(req,"accountId",accountId);
    if(getAccountId){
        return getAccountId.value();
    }
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    //搜索好友
    auto result=friendService_->findUser(accountId);
    if(!result){
        //搜索不到
        return makeErr(req,ErrorCode::USER_NOT_FOUND,"Failed to find the user");
    }
    auto profile=result.value();
    return makeOk(req,MsgType::SEARCH_USER_RESP,nlohmann::json{{"accountId",accountId},{"username",profile.username},{"nickname",profile.nickname},{"avatarUrl",profile.avatarUrl},{"signature",profile.signature}});

}

im::Response im::Imservice::handleListFriends(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    const auto& result=friendService_->listFriends(session.accountId_);
    
    nlohmann::json friendList=nlohmann::json::array();
    for(const auto& friendInfo:result){
        friendList.push_back(nlohmann::json{{"accountId",friendInfo.accountId},{"username",friendInfo.username},{"nickname",friendInfo.nickname},{"avatarUrl",friendInfo.avatarUrl},{"signature",friendInfo.signature}});
    }
    return makeOk(req,MsgType::LIST_FRIENDS_RESP,nlohmann::json{{"friends",friendList},{"count",friendList.size()}});
    
}
im::Response im::Imservice::handleRemoveFriend(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    auto result=friendService_->removeFriend(session.accountId_,targetAccountId);
    if(!result.ok()){
        if(result.status==storage::RepoStatus::NotFound){
            return makeErr(req,ErrorCode::NOT_FRIENDS,"The target user is not your friend");
        }
        return makeRepoError(req,result.status,result.message);
    }
    //推送好友事件通知对方被删除了好友
    auto pushResult=notifyFriendEvent(targetAccountId,"friendRemoved",nlohmann::json{{"accountId",session.accountId_},{"username",session.username_}});
    if(!pushResult.delivered()){//推送失败只记录日志，不回滚
        LOG_WARN("Failed to push friend removed event to"+targetAccountId);
    }
    return makeOk(req,MsgType::REMOVE_FRIEND_RESP,nlohmann::json{{"accountId",targetAccountId},{"removed",true}});
    
}
im::Imservice::AccountPushResult im::Imservice::pushToAccount(const std::string& targetAccountId,im::Response& push){
    //获取用户账号的全部ConnKey
    auto keys=sessionManager_.connKeysByAccountId(targetAccountId);
    if(keys.empty()){
        return AccountPushResult{};
    }
    decorate(push);
    //获取协议字符串
    auto pushString=encodeResponse(push);
    AccountPushResult pushResult;
    for(const auto& key:keys){//遍历key进行推送
        auto sendResult=sendPush(key,pushString);
        switch (sendResult)
        {//根据SendResult累加统计
        case SendResult::Ok:
            pushResult.sent++;
            break;
        case SendResult::Closed:
            pushResult.closed++;
            break;
        case SendResult::NoSuchConnection:
            pushResult.noSuchConnection++;
            break;
        case SendResult::Overloaded:
            pushResult.overloaded++;
            break;
        default:
            break;
        }
    }
    return pushResult;
}

im::Imservice::AccountPushResult im::Imservice::notifyFriendEvent(const std::string&targetAccountId,const std::string&event,nlohmann::json data){
    data["event"]=event;
    Response push{.ver=1,.req_id=0,.type=MsgType::FRIEND_EVENT_PUSH,.ok=true,.code=ErrorCode::OK,.data=std::move(data)};
    auto pushResult=pushToAccount(targetAccountId,push);
    return pushResult;
}
//好友请求接口
im::Response im::Imservice::handleSendFriendRequest(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    std::string targetAccountId;
    auto getAccountId=getStringField(req,"targetAccountId",targetAccountId);
    if(getAccountId){
        return getAccountId.value();
    }
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto result=friendService_->sendRequest(session.accountId_,targetAccountId,nowMs);
    if(!result.ok()){
        if(result.status==storage::RepoStatus::NotFound){
            return makeErr(req,ErrorCode::NO_SUCH_USER,"The target user was not found");
        }
        return makeRepoError(req,result.status,result.message);
    }
    auto pushResult=notifyFriendEvent(targetAccountId,"friendRequestReceived",nlohmann::json{{"requestId",result.value.value()},{"requesterAccountId",session.accountId_},{"username",session.username_}});
    if(!pushResult.delivered()){//推送失败只记录日志，不回滚
        LOG_WARN("Failed to push friend request event to"+targetAccountId);
    }
    return makeOk(req,MsgType::SEND_FRIEND_REQUEST_RESP,nlohmann::json{{"requestId",result.value.value()}});
}

im::Response im::Imservice::handleListFriendRequests(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    auto result=friendService_->listIncomingRequests(session.accountId_);
    if(result.empty()){
        return makeOk(req,MsgType::LIST_FRIEND_REQUEST_RESP,nlohmann::json{{"requests",nlohmann::json::array()},{"count",0}});
    }
    nlohmann::json friendRequestView=nlohmann::json::array();
    for(const auto& view:result){
        friendRequestView.push_back(nlohmann::json{{"requestId",view.requestId},{"accountId",view.requesterAccountId},{"username",view.username},{"nickname",view.nickname},{"avatarUrl",view.avatarUrl},{"createdAtMs",view.createdAtMs}});
    }
    return makeOk(req,MsgType::LIST_FRIEND_REQUEST_RESP,nlohmann::json{{"requests",friendRequestView},{"count",friendRequestView.size()}});
}
im::Response im::Imservice::handleAcceptFriendRequest(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    
    if(!friendService_){
        return makeErr(req,ErrorCode::INTERNAL,"FriendService is empty");
    }
    uint64_t requestId=0;
    if(req.body.contains("requestId")){
        if(req.body["requestId"].is_number_unsigned()){
            requestId=req.body["requestId"].get<uint64_t>();
        }
        else if(req.body["requestId"].is_number_integer()&&req.body["requestId"].get<int64_t>()>=0){
            requestId=static_cast<uint64_t>(req.body["requestId"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"Invalid requestId");
        }
    }
    else{
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing requestId");
    }
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto result=friendService_->acceptRequest(session.accountId_,requestId,nowMs);
    if(!result.ok()){
        if(result.status==storage::RepoStatus::NotFound){
            return makeErr(req,ErrorCode::FRIEND_REQUEST_NOT_FOUND,"The request was not found");
        }
        return makeRepoError(req,result.status,result.message);
    }
    auto pushResult=notifyFriendEvent(result.value.value().requestAccountId,"friendRequestAccepted",nlohmann::json{{"accountId",session.accountId_},{"username",session.username_}});
    if(!pushResult.delivered()){//推送失败只记录日志，不回滚
        LOG_WARN("Failed to push friend request accepted event to"+result.value.value().requestAccountId);
    }
    return makeOk(req,MsgType::ACCEPT_FRIEND_REQUEST_RESP,nlohmann::json{{"requestId",requestId}});
}
im::Response im::Imservice::handleRejectFriendRequest(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    
    uint64_t requestId=0;
    if(req.body.contains("requestId")){
        if(req.body["requestId"].is_number_unsigned()){
            requestId=req.body["requestId"].get<uint64_t>();
        }
        else if(req.body["requestId"].is_number_integer()&&req.body["requestId"].get<int64_t>()>=0){
            requestId=static_cast<uint64_t>(req.body["requestId"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"Invalid requestId");
        }
    }
    else{
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing requestId");
    }
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto result=friendService_->rejectRequest(session.accountId_,requestId,nowMs);
    if(!result.ok()){
        if(result.status==storage::RepoStatus::NotFound){
            return makeErr(req,ErrorCode::FRIEND_REQUEST_NOT_FOUND,"The request was not found");
        }
        return makeRepoError(req,result.status,result.message);
    }
    auto pushResult=notifyFriendEvent(result.value.value().requestAccountId,"friendRequestRejected",nlohmann::json{{"accountId",session.accountId_},{"username",session.username_}});
    if(!pushResult.delivered()){//推送失败只记录日志，不回滚
        LOG_WARN("Failed to push friend request rejected event to"+result.value.value().requestAccountId);
    }
    return makeOk(req,MsgType::REJECT_FRIEND_REQUEST_RESP,nlohmann::json{{"requestId",requestId}});
}

im::Response im::Imservice::handleConversationRead(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    std::string targetId;
    auto getTargetId=getStringField(req,"targetId",targetId);
    if(getTargetId){
        return getTargetId.value();
    }
    std::string conversationTypeString;
    auto getconversation=getStringField(req,"conversationType",conversationTypeString);
    if(getconversation){
        return getconversation.value();
    }
    storage::ConversationType conversationType;
    if(conversationTypeString=="direct"){
        if(!repos_.userRepo){
            return makeErr(req,ErrorCode::INTERNAL,"userRepo is not avaiable");
        }
        if(!repos_.userRepo->userExists(targetId)){
            return makeErr(req,ErrorCode::NO_SUCH_USER,"no such user");
        }
        conversationType=storage::ConversationType::Direct;
    }
    else if(conversationTypeString=="group"){
        conversationType=storage::ConversationType::Group;
        if(!groupManager_.isMember(targetId,session.accountId_)){
            return makeErr(req,ErrorCode::NOT_IN_GROUP,"the user is not in the group"+targetId);
        }
    }
    else{
        return makeErr(req,ErrorCode::BAD_REQUEST,"conversationType is invalid");
    }
    uint64_t readMsgId=0;
    if(req.body.contains("readMsgId")){
        if(req.body["readMsgId"].is_number_unsigned()){
            readMsgId=req.body["readMsgId"].get<uint64_t>();
        }
        else if(req.body["readMsgId"].is_number_integer()&&req.body["readMsgId"].get<int64_t>()>=0){
            readMsgId=static_cast<uint64_t>(req.body["readMsgId"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"Invalid readMsgId");
        }
    }
    else{
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing readMsgId");
    }
    if(!messageAckService_){
        return makeErr(req,ErrorCode::INTERNAL,"messageAckService is not avaiable");
    }
    auto result=messageAckService_->markConversationRead(session.accountId_,conversationType,targetId,readMsgId,nowMs());
    if(!result.ok()){
        return makeRepoError(req,result.status,result.message);
    }
    if(!result.value.has_value()){
        return makeErr(req,ErrorCode::INTERNAL,"ConversationReadResult value invalid");
    }
    auto conversationRes=result.value.value();
    return makeOk(req,MsgType::CONVERSATION_READ_RESP,nlohmann::json{{"conversationType",storage::conversationTypeToString(conversationType)},{"targetId",conversationRes.targetId},{"readMsgId",readMsgId},{"readAtMs",conversationRes.readAtMs},{"receiptUpdated",conversationRes.receiptUpdated}});
}

im::Response im::Imservice::handleConversationList(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);//校验已登录
    if(err){
        return err.value();
    }
    if(!conversationService_){
        //检查repos_离线存储是否存在
        return makeErr(req,ErrorCode::INTERNAL,"conversationService is not exist");
    }
    //解析limit
    size_t limit=parseLimit(req,"limit",20,200);
    auto result=conversationService_->listConversations(session.accountId_,limit);
    nlohmann::json conversationViewJson=nlohmann::json::array();
    for(const auto& view:result){
        nlohmann::json item{
        {"type",storage::conversationTypeToString(view.summary.type)},
        {"targetId",view.summary.targetId},
        {"lastMsgId",view.summary.lastMsgId},{"lastPreview",view.summary.lastPreview},
        {"lastSenderAccountId",view.summary.lastSenderAccountId},{"lastSenderUsername",view.summary.lastSenderUsername},
        {"lastTsMs",view.summary.lastTsMs},{"unreadCount",view.summary.unreadCount},
        {"lastReadMsgId",view.summary.lastReadMsgId}};
        if(view.summary.type==storage::ConversationType::Direct){
            item["targetUsername"] = view.targetUsername;
            item["targetNickname"] = view.targetNickname;
            item["targetAvatarUrl"] = view.targetAvatarUrl;
        }
        else if(view.summary.type == storage::ConversationType::Group){
            item["groupName"] = view.groupName;
            item["groupOwnerAccountId"] = view.groupOwnerAccountId;
        }
        conversationViewJson.emplace_back(std::move(item));
    }
    return makeOk(req,MsgType::CONVERSATION_LIST_RESP,nlohmann::json{{"conversations",conversationViewJson},{"count",conversationViewJson.size()}});
}
im::Response im::Imservice::handleSync(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    //同步消息限流
    if(rateLimiter_){
        auto limitResult=rateLimiter_->checkSync(session.accountId_,nowMs());
        auto resultOpt=checkRateLimitOrError(req,limitResult);
        if(resultOpt){
            return resultOpt.value();
        }
    }
    
    if(!messageSyncService_||!repos_.userRepo||!repos_.friendRepo){
        return makeErr(req,ErrorCode::INTERNAL,"messageSyncService");
    }
    size_t limit=parseLimit(req,"limit",50,imConfig_.maxSyncMessageLimit);
    size_t offlineLimit=parseLimit(req,"offlineLimit",100,imConfig_.maxOfflineIndexLimit);
    auto cursorsResult=parseSyncCursors(req,limit,imConfig_.maxSyncMessageLimit);
    if(!cursorsResult.ok){
        return makeErr(req,cursorsResult.code,cursorsResult.message);
    }
    if(cursorsResult.cursors.size()>imConfig_.maxSyncCursorCount){
        return makeErr(req,ErrorCode::BAD_REQUEST,"too many cursors");
    }
    for(const auto& cursor:cursorsResult.cursors){
        if(cursor.type==storage::ConversationType::Direct){
            if(!repos_.userRepo->userExists(cursor.targetId)){
                return makeErr(req,ErrorCode::USER_NOT_FOUND,"the user is not found");
            }
            if(!repos_.friendRepo->areFriends(session.accountId_,cursor.targetId)){
                return makeErr(req,ErrorCode::NOT_FRIENDS,"the user is not your friend");
            }
        }
        else if(cursor.type==storage::ConversationType::Group){
            if(!groupManager_.isMember(cursor.targetId,session.accountId_)){
                return makeErr(req,ErrorCode::NOT_IN_GROUP,"the user is not in the group");
            }
        }
    }
    auto result=messageSyncService_->sync(session.accountId_,cursorsResult.cursors,offlineLimit);
    nlohmann::json deltasJson=nlohmann::json::array();
    for(const auto& delta:result.deltas){
        deltasJson.emplace_back(nlohmann::json{{"conversationType",storage::conversationTypeToString(delta.type)},{"targetId",delta.targetId},{"messages",delta.messages},{"fromMsgId",delta.fromMsgId},{"latestMsgId",delta.latestMsgId},{"hasMore",delta.hasMore}});
    }
    nlohmann::json offlineIndexesJson=nlohmann::json::array();
    for(const auto&index:result.offlineIndexes){
        if(index.type==storage::OfflineMessageType::Direct){
            offlineIndexesJson.emplace_back(nlohmann::json{{"msgId",index.msgId},{"type","direct"},{"peerAccountId",index.peerAccountId}});
        }
        else if(index.type==storage::OfflineMessageType::Group){
            offlineIndexesJson.emplace_back(nlohmann::json{{"msgId",index.msgId},{"type","group"},{"groupId",index.groupId}});
        }
    }
    return makeOk(req,MsgType::SYNC_RESP,nlohmann::json{{"deltas",deltasJson},{"offlineIndexes",offlineIndexesJson},{"cursorCount",cursorsResult.cursors.size()},{"deltaCount",deltasJson.size()},{"offlineCount",offlineIndexesJson.size()}});
}
im::Response im::Imservice::handleMessageAck(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err){
        return err.value();
    }
    auto messageAck=parseMessageAck(req,imConfig_.maxAckBatchSize);
    if(!messageAck.ok){
        return makeErr(req,messageAck.code,messageAck.message);
    }
    if(!messageAckService_){
        return makeErr(req,ErrorCode::INTERNAL,"messageAckService is not avaiable");
    }
    storage::MessageAckResult ackResult;
    if(!messageAck.payload.msgIds.empty()){
        auto resultAckMessage=messageAckService_->ackMessages(session.accountId_,messageAck.payload.msgIds,nowMs());
        if(!resultAckMessage.ok()){
            return makeRepoError(req,resultAckMessage.status,resultAckMessage.message);
        }
        if(!resultAckMessage.value.has_value()){
            return makeErr(req,ErrorCode::INTERNAL,"messageAckResult value invalid");
        }
        ackResult=resultAckMessage.value.value();

    }
    if(!messageAck.payload.offlineMsgIds.empty()){
        auto resultAckOfflineMessage=messageAckService_->ackOfflineMessages(session.accountId_,messageAck.payload.offlineMsgIds);
        if(!resultAckOfflineMessage.ok()){
            return makeRepoError(req,resultAckOfflineMessage.status,resultAckOfflineMessage.message);
        }
    }
    return makeOk(req,MsgType::MESSAGE_ACK_RESP,nlohmann::json{{"requestedMsgCount",ackResult.requestedCount},{"ackedMsgCount",ackResult.ackedCount},{"ignoredMsgCount",ackResult.ignoredCount},{"OfflineMsgCount",messageAck.payload.offlineMsgIds.size()}});
}

//业务限流服务
im::Response im::Imservice::makeRateLimitError(const Request& req,const security::RateLimitResult& result){
    return makeErr(req,ErrorCode::RATE_LIMITED,"too many requests",nlohmann::json{{"retryAfterMs",result.retryAfterMs}});
}
std::optional<im::Response> im::Imservice::checkRateLimitOrError(const Request& req,const security::RateLimitResult& result){
    if(result.allowed){
        return std::nullopt;
    }
    return makeRateLimitError(req,result);
}