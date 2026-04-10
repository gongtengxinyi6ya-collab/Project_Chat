#include "im/ImService.h"
#include "third_party/json.hpp"
#include "TcpConnection.h"

explicit im::Imservice::Imservice(uint32_t supportedVer):supportedVer_(supportedVer){}

void im::Imservice::setSendToConnKey(SendToConnKeyFn fn){
    sendToConnKey_=std::move(fn);
}
void im::Imservice::onMessage(const std::shared_ptr<TcpConnection>&conn,const std::string &payload){
    ConnKey key=conn->fd();
    auto session=getOrCreateSession(key);
    auto req_or_resp=im::tryParse(payload);
    if(auto resp_ptr=std::get_if<im::Response>(&req_or_resp)){
        //请求解析失败，直接返回错误响应
        if(resp_ptr->ok==false){
            std::string resp_str=im::encodeResponse(*resp_ptr);
            sendToConnKey_(key,resp_str);
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
        sendToConnKey_(key,resp_str);
    }

}
void im::Imservice::onDisconnect(const std::shared_ptr<TcpConnection> & conn){
    ConnKey key=conn->fd();
    sessions_.erase(key);
}