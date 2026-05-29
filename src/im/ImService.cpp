#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"
#include "storage/UserRepo.h"
#include "storage/GroupRepo.h"
#include "storage/MessageRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "auth/AuthService.h"
#include "auth/AuthResult.h"
#include "security/PasswordHasher.h"

im::Imservice::Imservice(uint32_t supportedVer,const ImConfig& config):supportedVer_(supportedVer),imConfig_(config){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto &session=sessionManager_.getOrCreate(key);
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
    if(!sessionManager_.bindUser(key,username)){
        return makeErr(req,im::ErrorCode::INTERNAL,"Failed to bind user to session");
    }
    /*
    if(hasRepositories()){
        auto result=repos_.userRepo->createUser(username);
        if(result.status!=storage::RepoStatus::Ok&&result.status!=storage::RepoStatus::AlreadyExists){
            return makeRepoError(req,result.status,"Fail to create user");
        }
    }*/
    return makeOk(req,im::MsgType::AUTH_RESP);
}

std::optional<im::Response> im::Imservice::guardAuthenticated(const Request& req,const Session& session){
    if(session.state_==im::ConnState::Authed){
        return std::nullopt;
    }
    return makeErr(req,im::ErrorCode::NOT_AUTHED,"Unauthed, please authenticate first");
}
std::optional<im::Response> im::Imservice::guardInGroup(const Request& req,const Session& session,const std::string& groupId){
    if(!groupId.empty()&&session.joinedGroupIds_.count(groupId)){
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
    std::vector<storage::GroupRepo::GroupSnapshot> groups=repos_.groupRepo->listGroups();
    for(const auto& group:groups){
        auto members=repos_.groupRepo->listMembers(group.groupId);
        if(groupManager_.restoreGroup(group.groupId,group.groupName,group.owner,members)){
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
    //取目标
    if(req.to.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message recipient");
    }
    auto keys=sessionManager_.connKeysByUser(req.to);
    if(keys.empty()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"Recipient user is not online");
    }
    //取文本
    std::string content;
    if(auto errField=getStringField(req,"content",content)){
        return errField.value();
    }
    //构造推送消息
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::DM_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New direct message",.data=nlohmann::json{{"from",session.username_},{"to",req.to},{"content",content}}};
    decorate(pushMsg,std::nullopt,req.req_id);//推送消息也携带client_req_id，方便客户端关联请求和推送
    std::string payload=encodeResponse(pushMsg);
    for(const auto& targetKey:keys){
        SendResult res=sendPush(targetKey,payload);
        if(res!=SendResult::Ok){
            return makeErr(req,im::ErrorCode::BAD_REQUEST,"Failed to deliver message, recipient is offline or overloaded");
        }
    }
    return makeOk(req,im::MsgType::DM_RESP);

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
        resp.data["msg_id"]=nextMsgId_++;
    }
    resp.data["server_ts_ms"]=nowMs();
    if(clientReqId.has_value()){
        resp.data["client_req_id"]=*clientReqId;
    }
}
 im::Imservice::SendResult im::Imservice::sendPush(ConnKey target,const std::string& payload){
    
    if(sendToConnKey_){
        SendResult res=sendToConnKey_(target,payload);
        return res;
    }
    return SendResult::NoSuchConnection;
    
}
std::optional<std::string> im::Imservice::resolveTargetGroupId(const Request& req,const Session& session){
    if(req.body.contains("groupId")&&req.body["groupId"].is_string()){
        std::string groupId=req.body["groupId"];
        if(!groupId.empty()){
            return groupId;
        }
    }
    if(session.joinedGroupIds_.size()==1){
        return *session.joinedGroupIds_.begin();
    }
    return std::nullopt;
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
    std::string owner=session.username_;
    auto [success,groupIdOrErr]=groupManager_.createGroup(owner,groupName);
    if(!success){
        return makeErr(req,im::ErrorCode::INTERNAL,"Failed to create group:"+groupIdOrErr);
    }
    std::string groupId=groupIdOrErr;
    if(hasRepositories()){
        auto result=repos_.groupRepo->createGroupWithOwner(groupId,groupName,owner);
        if(result.status!=storage::RepoStatus::Ok&&result.status!=storage::RepoStatus::AlreadyExists){
            LOG_ERROR_CTX("repo create group failed",makeReqCtx(key,req,session,"Repo failed"));
            groupManager_.leaveGroup(groupId,owner);//回滚内存状态
            groupManager_.removeGroup(groupId);
            return makeRepoError(req,result.status,"failed to persist group");
        }
    }
    session.joinedGroupIds_.insert(groupId);
    return makeOk(req,im::MsgType::CREATE_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"groupName",groupName},{"owner",owner}});
}
im::Imservice::BroadcastResult im::Imservice::broadcastToGroup(const std::string& groupId,[[maybe_unused]]const std::string& fromUser,ConnKey senderkey,im::Response& push){
    BroadcastResult result;
    push.req_id=0;//群推送消息不需要req_id，由服务器生成唯一msg_id
    std::optional<uint64_t> msgId=std::nullopt;
    if(push.data.contains("msg_id")){
        msgId=push.data["msg_id"].get<uint64_t>();
    }
    decorate(push,msgId);//群推送消息也需要decorate添加msg_id和server_ts_ms等字段
    auto payload=encodeResponse(push);
    const auto& users=groupManager_.members(groupId);//根据群id取成员用户名列表
    for(const auto& user:users){
        const auto& keys=sessionManager_.connKeysByUser(user);
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

im::Response im::Imservice::handleJoin(const im::Request & req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);//登录门禁
    if(err.has_value()){
        return err.value();
    }
    //读取groupId
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    auto user=sessionManager_.usernameByConn(key);
    if(!user.has_value()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"user is not exist");
    }
    auto joinResult=groupManager_.joinGroup(groupId,user.value());
    if(joinResult==JoinResult::ERR_NO_SUCH_GROUP){
        return makeErr(req,im::ErrorCode::NO_SUCH_GROUP,"no such group");
    }
    if(joinResult==JoinResult::OK_ALREADY_IN){
        session.joinedGroupIds_.insert(groupId);//虽然已经在群里了，但为了防止session状态不一致，还是把群id加入session的joinedGroupIds_里
        return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"joined",false},{"alreadyIn",true}});
    }
    session.joinedGroupIds_.insert(groupId);
    if(hasRepositories()){
        auto result=repos_.groupRepo->addMember(groupId,user.value());
        if(result.status!=storage::RepoStatus::Ok&&result.status!=storage::RepoStatus::AlreadyExists){

            groupManager_.leaveGroup(groupId,user.value());//回滚内存状态
            session.joinedGroupIds_.erase(groupId);
            return makeRepoError(req,result.status,"filed to persist group member");
        }
    }
    return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"joined",true},{"alreadyIn",false}});
}

im::Response im::Imservice::handleLeave(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    auto user=sessionManager_.usernameByConn(key);
    if(!user.has_value()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"User is not exist");
    }
    QuitResult quitResult=groupManager_.leaveGroup(groupId,user.value());
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
        auto result = repos_.groupRepo->removeMember(groupId, user.value());

        if (result.status != storage::RepoStatus::Ok &&
            result.status != storage::RepoStatus::NotFound) {
            groupManager_.joinGroup(groupId, user.value());//回滚内存状态
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
    if(imConfig_.requireGroupIdForSend){//如果配置要求必须提供groupId字段
        if(!req.body.contains("groupId")||!req.body["groupId"].is_string()){
            return makeErr(req,im::ErrorCode::MISSING_FIELD,"groupId can not be empty");
        }
    }
    auto groupId=resolveTargetGroupId(req,session);
    if(!groupId.has_value()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing groupId field and no active group");
    }
    auto errInGroup=guardInGroup(req,session,groupId.value());
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
    auto user=sessionManager_.usernameByConn(key);
    if(!user.has_value()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"User is not exist");
    }
    if(!groupManager_.isMember(groupId.value(),user.value())){
        return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"The user is not in the group");
    }
    uint64_t serverTsMs=nowMs();
    uint64_t msgId=nextMessageId();
    if(hasRepositories()){//保存消息
        auto result=repos_.messageRepo->saveGroupMessage(msgId,groupId.value(),user.value(),content,serverTsMs);
        if(!result.ok()){
            return makeRepoError(req,result.status,"failed to save group message");
        }
    }
    //广播在线成员
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::GROUP_MSG_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New room message",.data=nlohmann::json{{"from",session.username_},{"groupId",groupId.value()},{"content",content},{"msg_id",msgId}}};
    BroadcastResult result=broadcastToGroup(groupId.value(),user.value(),key,pushMsg);
    saveOfflineForGroupMembers(groupId.value(),user.value(),msgId);
    return makeOk(req,im::MsgType::GROUP_MSG_RESP,nlohmann::json{{"groupId",groupId.value()},{"sent",result.sent},{"dropped",result.dropped()},{"noSuchConnection",result.noSuchConnection},{"closed",result.closed},{"overloaded",result.overloaded}});

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
    auto groupId=resolveTargetGroupId(req,session);
    if(!groupId.has_value()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing groupId field and no active group");
    }
    auto errInGroup=guardInGroup(req,session,groupId.value());
    if(errInGroup.has_value()){
        return errInGroup.value();
    }
    std::vector<std::string> members=groupManager_.members(groupId.value());
    return makeOk(req,im::MsgType::GROUP_MEMBERS_RESP,nlohmann::json{{"groupId",groupId.value()},{"count",members.size()},{"members",members}});

}

std::optional<std::string> im::Imservice::usernameByKey(ConnKey key)const{
    auto it=sessionManager_.find(key);
    if(it&&!it->username_.empty()){
        return it->username_;
    }
    return std::nullopt;
}
im::Response im::Imservice::handleListGroups(const Request& req,[[maybe_unused]]ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::vector<std::string> groups=groupManager_.groupsOfUser(session.username_);
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
        {
            im::Response resp=handleJoin(req,key,session);
            if(resp.ok&&resp.data.contains("alreadyIn")&&resp.data["alreadyIn"].get<bool>()==false){
                    std::string groupId=resp.data["groupId"];
                    LOG_INFO_CTX("user joined group",makeReqCtx(key,req,session,"JOIN_GROUP"));
                    im::Response event=makeOk(req,im::MsgType::GROUP_EVENT_PUSH,nlohmann::json{{"event","join"},{"user",session.username_},{"groupId",groupId}});
                    broadcastToGroup(groupId,session.username_,key,event);
                }
            return resp;
        }
        
        case im::MsgType::LEAVE_GROUP_REQ:
        {
            im::Response resp=handleLeave(req,key,session);
            if(resp.ok&&resp.data.contains("left")&&resp.data["left"].get<bool>()==true){
                    std::string groupId=resp.data["groupId"];
                    LOG_INFO_CTX("im leave group",makeRespCtx(key,req,resp,session,"LEAVE_GROUP"));
                    im::Response leaveEvent=makeOk(req,im::MsgType::GROUP_EVENT_PUSH,nlohmann::json{{"event","leave"},{"user",session.username_},{"groupId",groupId}});
                    broadcastToGroup(groupId,session.username_,key,leaveEvent);
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
    std::string groupId;
    if(auto errField=getStringField(req,"groupId",groupId)){
        return errField.value();
    }
    uint64_t beforeMsgId=0;
    if(req.body.contains("beforeMsgId")){
        if(req.body["beforeMsgId"].is_number_unsigned()){
            beforeMsgId=req.body["beforeMsgId"].get<uint64_t>();
        }
        else if(req.body["beforeMsgId"].is_number_integer()&&req.body["beforeMsgId"].get<int64_t>()>=0){
            beforeMsgId=static_cast<uint64_t>(req.body["beforeMsgId"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::BAD_REQUEST,"Invalid beforeMsgId");
        }
    }
    size_t limit=20;
    if(req.body.contains("limit")){
        
        if(req.body["limit"].is_number_unsigned()){
            limit=req.body["limit"].get<size_t>();
        }
        else if(req.body["limit"].is_number_integer()&&req.body["limit"].get<int64_t>()>0){
            limit=static_cast<size_t>(req.body["limit"].get<int64_t>());
        }
        else{
            return makeErr(req,im::ErrorCode::BAD_REQUEST,"Invalid limit");
        }
    }
    if(limit>100){//limit最大值限制
        limit=100;
    }
    auto user=sessionManager_.usernameByConn(key);
    if(!user){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"User is not exist");
    }
    if(!groupManager_.isMember(groupId,user.value())){
        return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"The user is not in the group");
    }
    if(!hasRepositories()||!repos_.messageRepo){
        return makeErr(req,im::ErrorCode::INTERNAL,"Message repository is not configured");
    }
    //调用群历史消息查询
    auto messages=repos_.messageRepo->listGroupMessages(groupId,beforeMsgId,limit);
    //返回JSON数组mwssages
    nlohmann::json messagesJson=nlohmann::json::array();
    for(const auto& msg:messages){
        messagesJson.push_back(nlohmann::json{{"msg_id",msg.messageId},{"group_id",msg.groupId},{"from",msg.from},{ "content",msg.content},{"server_ts_ms",msg.serverTsMs}});
    }
    return makeOk(req,im::MsgType::GROUP_HISTORY_RESP,nlohmann::json{{"groupId",groupId},{"messages",messagesJson}});

}

void im::Imservice::saveOfflineForGroupMembers(const std::string& groupId,const std::string& fromUser,uint64_t msgId){
    if(groupId.empty()||fromUser.empty()){
        return;
    }
    if(!repos_.offlineMessageRepo){
        return;
    }
    auto members=groupManager_.members(groupId);//获取群成员
    for(auto member:members){
        if(member!=fromUser){//跳过发送者
            auto keys=sessionManager_.connKeysByUser(member);
            if(keys.empty()){
                //用户各端都不在线
                auto result=repos_.offlineMessageRepo->saveOfflineMessage(member,msgId,groupId);
                if(!result.ok()){
                    LOG_WARN("Failed to save offlineMessage for"+member);
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
    std::string& username=session.username_;
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
            return makeErr(req,im::ErrorCode::BAD_REQUEST,"Invalid limit");
        }
    }
    if(limit>200){
        limit=200;//限制limit最大值
    }
    auto indexes=repos_.offlineMessageRepo->listOfflineMessage(username,limit);
    nlohmann::json indexJson=nlohmann::json::array();
    for(const auto& index:indexes){
        indexJson.emplace_back(nlohmann::json{{"msg_id",index.msgId},{"group_id",index.groupId}});
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
    auto result=repos_.offlineMessageRepo->ackOfflineMessage(session.username_,msgIds);
    if(result.ok()){
        return makeOk(req,MsgType::OFFLINE_ACK_RESP,nlohmann::json{{"acked",msgIds.size()}});
    }
    return makeErr(req,ErrorCode::BAD_REQUEST,"Failed to ack msgIds");
}


//登录注册接口

im::Response im::Imservice::handleRegister(const Request& req,[[maybe_unused]]ConnKey key,[[maybe_unused]]Session& session){
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
        return makeOk(req,MsgType::REGISTER_RESP);
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
    if(session.state_==ConnState::Authed){
        if(username==session.username_){
            return makeOk(req,MsgType::LOGIN_RESP);
        }
        else{
            return makeErr(req,ErrorCode::BAD_REQUEST,"username is different");
        }
    }
    if(!authService_){
        return makeErr(req,ErrorCode::INTERNAL,"authService is not exist");
    }
    auto result=authService_->login(username,password);
    if(!result.ok){
        if(result.status==auth::AuthStatus::UserNotFound){
            return makeErr(req,ErrorCode::USER_NOT_FOUND,"User is not exist");
        }
        if(result.status==auth::AuthStatus::BadPassword){
            return makeErr(req,ErrorCode::BAD_PASSWORD,"Password is incorrect");
        }
        if(result.status==auth::AuthStatus::UserDisabled){
            return makeErr(req,ErrorCode::BAD_REQUEST,"User is disabled");
        }
        return makeErr(req,ErrorCode::INTERNAL,"Internal error");
    }
    if(!sessionManager_.bindUser(key,username)){
        return makeErr(req,ErrorCode::INTERNAL,"Failed to bind user");
    }
    session.userId_=result.user.value().userId;
    return makeOk(req,MsgType::LOGIN_RESP,nlohmann::json{{"userId",session.userId_},{"username",username},{"token",result.issuedToken.value().rawToken},{"expireAtMs",result.issuedToken.value().expireAtMs}});
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
    if(!sessionManager_.bindUser(key,result.user.value().username)){
        return makeErr(req,ErrorCode::INTERNAL,"Failed to bind user");
    }
    session.userId_=result.user.value().userId;
    return makeOk(req,MsgType::TOKEN_LOGIN_RESP,nlohmann::json{{"userId",session.userId_},{"username",session.username_},{"expireAtMs",result.tokenExpireAtMs.value()}});
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
            session.username_.clear();
            session.state_=ConnState::Connected;
            session.joinedGroupIds_.clear();
            session.userId_=0;
        }

        return makeOk(req,MsgType::LOGOUT_RESP,nlohmann::json{{"username",username},{"revoked",result.revokedNow},{"alreadyRevoked",result.alreadyLoggedOut}});
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
        return makeOk(req,MsgType::GET_PROFILE_RESP,nlohmann::json{{"userId",userProfile.userId},{"username",userProfile.username},{"nickname",userProfile.nickname},{"avatarUrl",userProfile.avatarUrl},{"signature",userProfile.signature},{"updateAtMs",userProfile.updateAtMs}});
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
