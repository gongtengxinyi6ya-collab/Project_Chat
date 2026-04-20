#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"

im::Imservice::Imservice(uint32_t supportedVer):supportedVer_(supportedVer){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto &session=sessionManager_.getOrCreate(key);
    auto req_or_resp=im::tryParse(payload);
    if(auto resp_ptr=std::get_if<im::Response>(&req_or_resp)){
        //请求解析失败，直接返回错误响应
        if(resp_ptr->ok==false){
            decorate(*resp_ptr);
            std::string resp_str=im::encodeResponse(*resp_ptr);
            if(sendToConnKey_){
                sendToConnKey_(key,resp_str);
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
            case im::MsgType::JOIN_REQ:
            {
                resp=handleJoin(*req_ptr,key,session);
                if(resp.ok){
                im::Response event=makeOk(*req_ptr,im::MsgType::ROOM_EVENT_PUSH,nlohmann::json{{"event","join"},{"user",session.username_},{"room",session.activeRoom_}});
                broadcastToRoom(session.activeRoom_,key,event);
                }
                break;
            }
            case im::MsgType::LEAVE_REQ:{
                std::string oldRoom=session.activeRoom_;
                resp=handleLeave(*req_ptr,key,session);
                if(resp.ok){
                    im::Response leaveEvent=makeOk(*req_ptr,im::MsgType::ROOM_EVENT_PUSH,nlohmann::json{{"event","leave"},{"user",session.username_},{"room",oldRoom}});
                    broadcastToRoom(oldRoom,key,leaveEvent);
                }
                break;
            }
            case im::MsgType::ROOM_MSG_REQ:
                resp=handleRoomMsg(*req_ptr,key,session);
                break;
            case im::MsgType::ROOM_MEMBERS_REQ:
                resp=handleRoomMembers(*req_ptr,key,session);
                break;
            default:
                resp=im::makeErr(*req_ptr,im::ErrorCode::UNKNOWN_TYPE,"Unknown message type");
                break;
        }
        decorate(resp,req_ptr->req_id);
        std::string resp_str=im::encodeResponse(resp);
        if(sendToConnKey_ ){
            sendToConnKey_(key,resp_str);
        }
    }

}
void im::Imservice::onDisconnect(const std::shared_ptr<TcpConnection> & conn){
    ConnKey key=conn->fd();
    if(auto it=sessionManager_.find(key)){
        roomManager_.removeKeyEverywhere(key);
        sessionManager_.unbindUser(key);
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
    
    if(!sessionManager_.bindUser(key,username)){
        return makeErr(req,im::ErrorCode::USER_EXISTS,"User already exist");
    }
    return makeOk(req,im::MsgType::AUTH_RESP);
}

std::optional<im::Response> im::Imservice::guardAuthenticated(const Request& req,const Session& session){
    if(session.state_==im::ConnState::Authed||!session.activeRoom_.empty()||!session.rooms_.empty()){
        return std::nullopt;
    }
    return makeErr(req,im::ErrorCode::NOT_AUTHED,"Unauthed, please authenticate first");
}
std::optional<im::Response> im::Imservice::guardInRoom(const Request& req,const Session& session){
    if(!session.activeRoom_.empty()){
        return std::nullopt;
    }
    return makeErr(req,im::ErrorCode::NOT_IN_ROOM,"Not in room,please join the room first");
}
std::string im::Imservice::resolveRoomOrActive(const Request& req,const Session& session,const char* fieldName){
    if(req.body.contains(fieldName)&&req.body[fieldName].is_string()){
        return req.body[fieldName];
    }
    if(!session.activeRoom_.empty()){
        return session.activeRoom_;
    }
    return "";
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
    auto it=sessionManager_.connKeyByUser(req.to);
    if(!it.has_value()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"Recipient user does not exist");
    }
    ConnKey targetKey=it.value();
    //取文本
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    //构造推送消息
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::DM_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New direct message",.data=nlohmann::json{{"from",session.username_},{"to",req.to},{"content",content}}};
    
    if(!sendPush(targetKey,pushMsg,req.req_id)){
        return makeOk(req,im::MsgType::DM_RESP,nlohmann::json{{"to","..."},{"delivered",false}});
    }
    return makeOk(req,im::MsgType::DM_RESP);

}

im::Response im::Imservice::handleListUsers(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    const std::vector<std::string>& users=sessionManager_.onLineUsers();
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


//房间接口


void im::Imservice::broadcastToGroup(const std::string& groupId,const std::string& fromUser,ConnKey senderkey,const im::Response& push){
    const auto& users=groupManager_.members(groupId);//根据群id取成员用户名列表
    for(const auto& user:users){
        const auto& keys=sessionManager_.connKeysByUser(user);
        for(const auto& key:keys){
            if(key!=senderkey){
                sendPush(key,push);
            }
        }
    }
   
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
        return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId}});
    }
    session.joinedGroupIds_.insert(groupId);
    return makeOk(req,im::MsgType::JOIN_GROUP_RESP,nlohmann::json{{"groupId",groupId}});
}

im::Response im::Imservice::handleLeave(const im::Request& req,ConnKey key,Session& session){
    auto err=guardAuthenticated(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::string room=resolveRoomOrActive(req,session,"room");
    if(room.empty()||!session.rooms_.count(room)){
        return makeErr(req,im::ErrorCode::NOT_IN_ROOM,"Not in the room");
    }
    removeFromRoom(key,session,room);

    return makeOk(req,im::MsgType::LEAVE_RESP,nlohmann::json{{"room",room},{"active_room",session.activeRoom_}});
}
im::Response im::Imservice::handleRoomMsg(const im::Request &req ,ConnKey key,Session& session){
    auto err=guardInRoom(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    std::string room=resolveRoomOrActive(req,session,"room");
    if(!room.empty()&&!session.rooms_.count(room)){
        return makeErr(req,im::ErrorCode::NOT_IN_ROOM,"Not in room");
    }
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::ROOM_MSG_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New room message",.data=nlohmann::json{{"from",session.username_},{"room",room},{"content",content}}};
    decorate(pushMsg,req.req_id);
    std::string pushStr=im::encodeResponse(pushMsg);
    auto members=roomManager_.members(room);
    size_t fanout=0;
    if(members.size()>0){
        fanout=members.size()-1;
    }
    for(const ConnKey& memberKey:members){
        if(memberKey!=key){
            sendToConnKey_(memberKey,pushStr);
        }
    }
    return makeOk(req,im::MsgType::ROOM_MSG_RESP,nlohmann::json{{"room",room},{"fanout",fanout}});

}
im::Response im::Imservice::handleRoomMembers(const im::Request& req,ConnKey key,Session& session){
    auto err=guardInRoom(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::string room=resolveRoomOrActive(req,session,"room");
    
    auto keys=roomManager_.members(room);
    if(keys.size()>0){
        std::vector<std::string> usernames;
        for(const auto& it:keys){
            auto name=usernameByKey(it);
            if(name.has_value()){
                usernames.push_back(name.value());
            }
        }
        return makeOk(req,im::MsgType::ROOM_MEMBERS_RESP,nlohmann::json{{"room",room},{"count",keys.size()},{"members",usernames}});
    }
    return makeOk(req,im::MsgType::ROOM_MEMBERS_RESP,nlohmann::json{{"room",room},{"count",0},{"members",std::vector<std::string>{}}});

}

std::optional<std::string> im::Imservice::usernameByKey(ConnKey key)const{
    auto it=sessionManager_.find(key);
    if(it&&!it->username_.empty()){
        return it->username_;
    }
    return std::nullopt;
}