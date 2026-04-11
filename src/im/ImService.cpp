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
            default:
                resp={.ver=req_ptr->ver,.req_id=req_ptr->req_id,.type=req_ptr->type,.ok=false,.code=im::ErrorCode::UNKNOWN_TYPE,.msg="Unknown message type",.data=nlohmann::json{}};
        }
        std::string resp_str=im::encodeResponse(resp);
        if(sendToConnKey_   ){
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
    sessions_.erase(key);

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
    if(!session.username_.empty()&&userConnMap_[session.username_]==key){
        userConnMap_.erase(session.username_);
    }
}
im::Response im::Imservice::handleDm(const im::Request& req,ConnKey key,Session& session){
    guarddAuthed(req,session);
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
    std::string pushStr=im::encodeResponse(pushMsg);
    sendToConnKey_(targetKey,pushStr);
    return makeOk(req,im::MsgType::DM_RESP);

}