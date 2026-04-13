#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"

im::Imservice::Imservice(uint32_t supportedVer):supportedVer_(supportedVer){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto &session=getOrCreateSession(key);
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
                resp=handleJoin(*req_ptr,key,session);
            case im::MsgType::LEAVE_REQ:
                resp=handleLeave(*req_ptr,key,session);
            case im::MsgType::ROOM_MSG_REQ:
                resp=handleRoomMsg(*req_ptr,key,session);
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
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        cleanupUserConn(key,it->second);
    }
    removeFromRoom(key,it->second);
    sessions_.erase(key);

}

im::Session& im::Imservice::getOrCreateSession(ConnKey key){
    auto it=sessions_.find(key);
    if(it!=sessions_.end()){
        return it->second;
    }
    //不存在则创建
    Session session;
    sessions_[key]=session;
    return sessions_[key];
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
    if(userConnMap_.count(username)&&userConnMap_[username]!=key){
        return makeErr(req,im::ErrorCode::USER_EXISTS,"Username already in use");
    }
    session.state_=im::ConnState::Authed;
    session.username_=username;
    userConnMap_[username]=key;
    return makeOk(req,im::MsgType::AUTH_RESP);
}

std::optional<im::Response> im::Imservice::guarddAuthed(const im::Request& req,const Session& session){
    if(session.state_!=im::ConnState::Authed){
        return makeErr(req,im::ErrorCode::NOT_AUTHED,"Unauthorized: Please authenticate first");
    }
    return std::nullopt;
}
void im::Imservice::cleanupUserConn(ConnKey key,const Session &session){
    
    if(!session.username_.empty()){
        auto it=userConnMap_.find(session.username_);
        if(it!=userConnMap_.end()&&it->second==key){
            userConnMap_.erase(session.username_);
        }
    }
}
im::Response im::Imservice::handleDm(const im::Request& req,ConnKey key,Session& session){
    auto err=guarddAuthed(req,session);
    if(err.has_value()){
        return err.value();
    }

    //取目标
    if(req.to.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message recipient");
    }
    auto it=userConnMap_.find(req.to);
    if(it==userConnMap_.end()){
        return makeErr(req,im::ErrorCode::NO_SUCH_USER,"Recipient user does not exist");
    }
    ConnKey targetKey=it->second;
    //取文本
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    //构造推送消息
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::DM_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New direct message",.data=nlohmann::json{{"from",session.username_},{"to",req.to},{"content",content}}};
    decorate(pushMsg,req.req_id);
    std::string pushStr=im::encodeResponse(pushMsg);
    if(!sendToConnKey_(targetKey,pushStr)){
        return makeOk(req,im::MsgType::DM_RESP,nlohmann::json{{"to","..."},{"delivered",false}});
    }
    return makeOk(req,im::MsgType::DM_RESP);

}

im::Response im::Imservice::handleListUsers(const im::Request& req,ConnKey key,Session& session){
    auto err=guarddAuthed(req,session);
    if(err.has_value()){
        return err.value();
    }
    std::vector<std::string> users;
    for(const auto& pair:userConnMap_){
        users.push_back(pair.first);
    }
    return makeOk(req,im::MsgType::LIST_USERS_RESP,nlohmann::json{{"users",users}});
}

im::Response im::Imservice::handleEcho(const im::Request& req,ConnKey key,Session& session){
    auto err=guarddAuthed(req,session);
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

//房间接口
void im::Imservice::removeFromRoom(ConnKey key,Session& session){
    if(!session.room_.empty()){
        roomMembers_[session.room_].erase(key);
        if(roomMembers_[session.room_].empty())
            roomMembers_.erase(session.room_);
        session.room_.clear();
        session.state_=im::ConnState::Authed;
    }
}

im::Response im::Imservice::handleJoin(const im::Request & req,ConnKey key,Session& session){
    auto err=guarddAuthed(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(!req.body.contains("room")||!req.body["room"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing room name");
    }
    std::string room=req.body["room"].get<std::string>();
    if(room.empty()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Room name cannot be empty");
    }
    if(session.state_==im::ConnState::InRoom&&session.room_==room){
        return makeOk(req,im::MsgType::JOIN_RESP);
    }
    if(session.state_==im::ConnState::InRoom&&session.room_!=room){
        removeFromRoom(key,session);
    }
    roomMembers_[room].insert(key);
    session.room_=room;
    session.state_=im::ConnState::InRoom;
    return makeOk(req,im::MsgType::JOIN_RESP);

}

im::Response im::Imservice::handleLeave(const im::Request& req,ConnKey key,Session& session){
    auto err=guarddAuthed(req,session);
    if(err.has_value()){
        return err.value();
    }
    if(session.state_!=im::ConnState::InRoom){
        return makeOk(req,im::MsgType::LEAVE_RESP);
    }
    removeFromRoom(key,session);
    return makeOk(req,im::MsgType::LEAVE_RESP);
}
im::Response im::Imservice::handleRoomMsg(const im::Request &req ,ConnKey key,Session& session){
    if(session.state_!=im::ConnState::InRoom){
        return makeErr(req,im::ErrorCode::BAD_REQUEST,"You must join a room to send messages");
    }
    if(!req.body.contains("content")||!req.body["content"].is_string()){
        return makeErr(req,im::ErrorCode::MISSING_FIELD,"Missing message content");
    }
    std::string content=req.body["content"].get<std::string>();
    std::string room=session.room_;
    im::Response pushMsg{.ver=1,.req_id=0,.type=im::MsgType::ROOM_MSG_PUSH,.ok=true,.code=im::ErrorCode::OK,.msg="New room message",.data=nlohmann::json{{"from",session.username_},{"room",room},{"content",content}}};
    decorate(pushMsg,req.req_id);
    std::string pushStr=im::encodeResponse(pushMsg);
    auto members=roomMembers_[room];
    for(ConnKey memberKey:members){
        if(memberKey!=key){
            sendToConnKey_(memberKey,pushStr);
        }
    }
    return makeOk(req,im::MsgType::ROOM_MSG_RESP);

}