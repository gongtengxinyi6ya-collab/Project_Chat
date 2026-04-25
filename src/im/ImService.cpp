#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"

im::Imservice::Imservice(uint32_t supportedVer,const ImConfig& config):supportedVer_(supportedVer),imConfig_(config){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto &session=sessionManager_.getOrCreate(key);
    auto req_or_resp=im::tryParse(payload);
    LOG_INFO_CTX("im request in",makeReqCtx(key,std::get_if<im::Request>(&req_or_resp)?*std::get_if<im::Request>(&req_or_resp):im::Request{},session,"REQ_IN"));
    if(auto resp_ptr=std::get_if<im::Response>(&req_or_resp)){
        //请求解析失败，直接返回错误响应
        if(resp_ptr->ok==false){//解析失败的响应
            decorate(*resp_ptr);
            LOG_WARN_CTX("im parse failed",makeRespCtx(key,im::Request{},*resp_ptr,session,"PARSE_ERR"));
            std::string resp_str=im::encodeResponse(*resp_ptr);
            if(sendToConnKey_){//发送错误响应
                if(!sendToConnKey_(key,resp_str)){
                    LOG_ERROR_CTX("Failed to send parse error response",makeRespCtx(key,im::Request{},*resp_ptr,session,"PARSE_ERR_SEND_FAIL"));
                }
            }
        }
    }
    else if(auto req_ptr=std::get_if<im::Request>(&req_or_resp)){
        im::Response resp;
        switch(req_ptr->type){
            case im::MsgType::AUTH_REQ:
                resp=handleAuth(*req_ptr,key,session);
                break;
            case im::MsgType::ECHO_REQ:
                resp=handleEcho(*req_ptr,key,session);
                break;
            case im::MsgType::DM_REQ:
                resp=handleDm(*req_ptr,key,session);
                break;
            case im::MsgType::LIST_USERS_REQ:
                resp=handleListUsers(*req_ptr,key,session);
                break;
            case im::MsgType::CREATE_GROUP_REQ:
                resp=handleCreateGroup(*req_ptr,key,session);
                break;
            case im::MsgType::JOIN_GROUP_REQ:
            {
                resp=handleJoin(*req_ptr,key,session);
                if(resp.ok&&resp.data.contains("alreadyIn")&&resp.data["alreadyIn"].get<bool>()==false){
                    std::string groupId=resp.data["groupId"];
                    LOG_INFO_CTX("user joined group",makeReqCtx(key,*req_ptr,session,"JOIN_GROUP"));
                    im::Response event=makeOk(*req_ptr,im::MsgType::GROUP_EVENT_PUSH,nlohmann::json{{"event","join"},{"user",session.username_},{"groupId",groupId}});
                    broadcastToGroup(groupId,session.username_,key,event);
                }
                break;
            }
            case im::MsgType::LEAVE_GROUP_REQ:{
                resp=handleLeave(*req_ptr,key,session);
                if(resp.ok){
                    std::string groupId=resp.data["groupId"];
                    LOG_INFO_CTX("im leave group",makeRespCtx(key,*req_ptr,resp,session,"LEAVE_GROUP"));
                    im::Response leaveEvent=makeOk(*req_ptr,im::MsgType::GROUP_EVENT_PUSH,nlohmann::json{{"event","leave"},{"user",session.username_},{"groupId",groupId}});
                    broadcastToGroup(groupId,session.username_,key,leaveEvent);
                }
                break;
            }
            case im::MsgType::GROUP_MSG_REQ:
                resp=handleGroupMsg(*req_ptr,key,session);
                LOG_INFO_CTX("im group msg req",makeReqCtx(key,*req_ptr,session,"GROUP_MSG_REQ"));
                break;
            case im::MsgType::GROUP_MEMBERS_REQ:
                resp=handleGroupMembers(*req_ptr,key,session);
                break;
            case im::MsgType::LIST_GROUPS_REQ:
                resp=handleListGroups(*req_ptr,key,session);
                break;
            default:
                resp=im::makeErr(*req_ptr,im::ErrorCode::UNKNOWN_TYPE,"Unknown message type");
                break;
        }
        decorate(resp,req_ptr->req_id);
        LOG_INFO_CTX("im request out",makeRespCtx(key,*req_ptr,resp,session,"RESP_OUT"));
        std::string resp_str=im::encodeResponse(resp);
        if(sendToConnKey_ ){
            sendToConnKey_(key,resp_str);
        }
    }

}
void im::Imservice::onDisconnect(const std::shared_ptr<TcpConnection> & conn){
    ConnKey key=conn->fd();
    if(auto it=sessionManager_.find(key)){
        sessionManager_.unbindConn(key);
        sessionManager_.erase(key);
    }
}


im::Response im::Imservice::handleAuth(const Request&req,ConnKey key,Session& session){
    if(!req.body.contains("user")){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing username field");
    }
    std::string username=req.body["user"].get<std::string>();
    if(username.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"username cannot be empty");
    }
    if(session.state_==im::ConnState::Authed&&session.username_==username){//返回幂等OK
        return makeOk(req,im::MsgType::AUTH_RESP);
    }
    if(session.state_==im::ConnState::Authed&&session.username_!=username){
        return makeErr(req,im::ErrorCode::USER_EXISTS,"User already authenticated with a different username");
    }
    
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

im::Response im::Imservice::handleDm(const im::Request& req,ConnKey key,Session& session){
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
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    //构造推送消息
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::DM_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New direct message",.data=nlohmann::json{{"from",session.username_},{"to",req.to},{"content",content}}};
    for(const auto& targetKey:keys){
        if(!sendPush(targetKey,pushMsg,req.req_id)){
            return makeOk(req,im::MsgType::DM_RESP,nlohmann::json{{"to","..."},{"delivered",false}});
        }
    }
    return makeOk(req,im::MsgType::DM_RESP);

}

im::Response im::Imservice::handleListUsers(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::vector<std::string>users=sessionManager_.onLineUsers();
    return makeOk(req,im::MsgType::LIST_USERS_RESP,nlohmann::json{{"users",users}});
}

im::Response im::Imservice::handleEcho(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    return makeOk(req,im::MsgType::ECHO_RESP,req.body);
}
uint64_t im::Imservice::nowMs()const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void im::Imservice::decorate(im::Response& resp,std::optional<uint64_t> clientReqId){
    resp.data["msg_id"]=nextMsgId_++;
    resp.data["server_ts_ms"]=nowMs();
    if(clientReqId.has_value()){
        resp.data["client_req_id"]=*clientReqId;
    }
}
bool im::Imservice::sendPush(ConnKey target,Response push,std::optional<uint64_t> clientReqid){
    push.req_id=0;
    decorate(push,clientReqid);
    auto payload=encodeResponse(push);
    if(sendToConnKey_&&sendToConnKey_(target,payload)){
        return true;
    }
    return false;
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

im::Response im::Imservice::handleCreateGroup(const Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(!req.body.contains("name")||!req.body["name"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing groupName");
    }
    if(req.from.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing from name");
    }
    std::string groupName=req.body["name"];
    if(groupName.size()>imConfig_.maxGroupNameLen){
        return makeErr(req,im::ErrorCode::GROUP_NAME_INVALID,"Group name is too long");
    }
    std::string owner=session.username_;
    auto [success,groupIdOrErr]=groupManager_.createGroup(owner,groupName);
    if(!success){
        return makeErr(req,im::ErrorCode::INTERNAL,"Failed to create group:"+groupIdOrErr);
    }
    std::string groupId=groupIdOrErr;
    session.joinedGroupIds_.insert(groupId);
    return makeOk(req,im::MsgType::CREATE_GROUP_RESP,nlohmann::json{{"groupId",groupId}});
}
size_t im::Imservice::broadcastToGroup(const std::string& groupId,const std::string& fromUser,ConnKey senderkey,const im::Response& push){
    const auto& users=groupManager_.members(groupId);//根据群id取成员用户名列表
    size_t fanout=0;
    for(const auto& user:users){
        const auto& keys=sessionManager_.connKeysByUser(user);
        for(const auto& key:keys){
            if(key!=senderkey){
                sendPush(key,push);
                fanout++;

            }
        }
    }
    return fanout;
   
}

im::Response im::Imservice::handleJoin(const im::Request & req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(!req.body.contains("groupId")||!req.body["groupId"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing groupId name");
    }
    std::string groupId=req.body["groupId"].get<std::string>();
    if(groupId.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"groupId cannot be empty");
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
        return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"alreadyIn",true}});
    }
    session.joinedGroupIds_.insert(groupId);
    return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId},{"alreadyIn",false}});
}

im::Response im::Imservice::handleLeave(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(!req.body.contains("groupId")||!req.body["groupId"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"groupId can not be empty");
    }
    std::string groupId=req.body["groupId"];
    if(groupId.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"GroupId is empty");
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
        return makeErr(req,im::ErrorCode::NOT_IN_GROUP,"The user is not in the group");
    }
    session.joinedGroupIds_.erase(groupId);
    return makeOk(req,im::MsgType::LEAVE_GROUP_RESP,nlohmann::json{{"groupId",groupId}});
}
im::Response im::Imservice::handleGroupMsg(const im::Request &req ,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
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
    auto err=guardInGroup(req,session,groupId.value());
    if(err.has_value()){
        return err.value();
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
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::GROUP_MSG_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New room message",.data=nlohmann::json{{"from",session.username_},{"groupId",groupId},{"content",content}}};
    size_t fanout=broadcastToGroup(groupId.value(),user.value(),key,pushMsg);
    return makeOk(req,im::MsgType::GROUP_MSG_RESP,nlohmann::json{{"groupId",groupId.value()},{"fanout",fanout}});

}
im::Response im::Imservice::handleGroupMembers(const im::Request& req,ConnKey key,Session& session){
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
    auto err=guardInGroup(req,session,groupId.value());
    if(err.has_value()){
        return err.value();
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
im::Response im::Imservice::handleListGroups(const Request& req,ConnKey key,Session& session){
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
    ctx.msgType=static_cast<uint32_t>(im::msgTypeToInt(req.type));
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
    if(resp.data.contains("fanout")&&resp.data["fanout"].is_number_unsigned()){
        ctx.fanout=resp.data["fanout"];
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