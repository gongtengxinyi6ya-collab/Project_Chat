#include "logger/LogContext.h"

bool LogContext::empty()const{
    if(connFd||user||groupId||msgId||reqId||msgType||errCode||event||fanout){
        return false;
    }
    return true;
}
std::string LogContext::toKvString()const{
    std::string out;
    bool first=true;
    auto appendKv=[&](std::string_view k,std::string_view v){
        if(!first){
            out.append(" ");
        }
        out.append(k);
        out.append("=");
        out.append(v);
        first=false;
    };
    auto appendKvNum=[&](std::string_view k,auto v){
        appendKv(k,std::to_string(v));
    };
    if(event){
        appendKv("event",*event);
    }
    if(connFd){
        appendKvNum("connFd",*connFd);
    }
    if(user){
        appendKv("user",*user);
    }
    if(groupId){
        appendKv("groupId",*groupId);
    }
    if(msgId){
        appendKvNum("msgId",*msgId);
    }
    if(reqId){
        appendKvNum("reqId",*reqId);
    }
    if(msgType){
        appendKvNum("msgType",*msgType);
    }
    if(errCode){
        appendKvNum("errCode",*errCode);
    }
    if(fanout){
        appendKvNum("fanout",*fanout);
    }
    return out;
}