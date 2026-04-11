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
        return im::Response{.ver=1,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::MISSING_FIELD,.msg="missing required field",.data=nlohmann::json{}};
    }
    std::string username=req.body["user"].get<std::string>();
    if(username.empty()){
        return im::Response{.ver=1,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::MISSING_FIELD,.msg="username cannot be empty",.data=nlohmann::json{}};
    }
    if(session.state_==im::ConnState::Authed&&session.username_==username){//返回幂等OK
        return im::Response{.ver=1,.req_id=req.req_id,.type=im::MsgType::AUTH_RESP,.ok=true,.code=im::ErrorCode::OK,.msg="Already authenticated",.data=nlohmann::json{}}; 
    }
    if(session.state_==im::ConnState::Authed&&session.username_!=username){
        return im::Response{.ver=1,.req_id=req.req_id,.type=im::MsgType::AUTH_RESP,.ok=false,.code=im::ErrorCode::USER_EXISTS,.msg="User already authenticated with a different username",.data=nlohmann::json{}};
    }
    session.state_=im::ConnState::Authed;
    session.username_=username;
    userConnMap_[username]=key;
    return im::Response{.ver=1,.req_id=req.req_id,.type=im::MsgType::AUTH_RESP,.ok=true,.code=im::ErrorCode::OK,.msg="Authentication successful",.data=nlohmann::json{}}; 
}

std::optional<im::Response> im::Imservice::guarddAuthed(const im::Request& req,const Session& session){
    if(session.state_!=im::ConnState::Authed){
        return im::Response{.ver=1,.req_id=req.req_id,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::NOT_AUTHED,.msg="Unauthorized: Please authenticate first",.data=nlohmann::json{}};
    }
    return std::nullopt;
}
void im::Imservice::cleanupUserConn(ConnKey key,const Session &session){
    if(!session.username_.empty()&&userConnMap_[session.username_]==key){
        userConnMap_.erase(session.username_);
    }
}