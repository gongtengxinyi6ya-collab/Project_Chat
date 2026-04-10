#include "im/ImCodec.h"

std::variant<im::Request,im::Response> im::tryParse(std::string_view payload){
    try{
        //JSON parse
        auto j=nlohmann::json::parse(payload);
        //校验ver/type/req_id
        if(!j.contains("ver")||!j.contains("type")||!j.contains("req_id")){
            return im::Response{.ver=1,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::MISSING_FIELD,.msg="Missing required field",.data=nlohmann::json{}};
        }
        uint32_t ver=j["ver"].get<uint32_t>();
        if(ver!=1){
            return im::Response{.ver=ver,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::UNSUPPORTED_VER,.msg="Unsupported protocol version",.data=nlohmann::json{}};
        }
        uint32_t type_int=j["type"].get<uint32_t>();
        auto type_opt=im::msgTypeFromInt(type_int);
        if (!type_opt.has_value()){
            return im::Response{.ver=ver,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::UNKNOWN_TYPE,.msg="Unknown message type",.data=nlohmann::json{}};
        }
        im::MsgType type=type_opt.value();
        uint64_t req_id=j["req_id"].get<uint64_t>();
        //构造Request
        im::Request req{.ver=ver,.type=type,.req_id=req_id,.seq=0,.from="",.to="",.body=j};
        //可选字段from/to/seq
        if(j.contains("seq")){
            req.seq=j["seq"].get<uint64_t>();
        }
        if(j.contains("from")){
            req.from=j["from"].get<std::string>();
        }
        if(j.contains("to")){
            req.to=j["to"].get<std::string>();
        }
        return req;
    }catch(const nlohmann::json::parse_error& e){
        return im::Response{.ver=1,.req_id=0,.type=im::MsgType::ERR,.ok=false,.code=im::ErrorCode::BAD_JSON,.msg="Invalid JSON format",.data=nlohmann::json{}};
    }
}

std::string im::encodeResponse(const im::Response& resp){
    nlohmann::json j;
    j["ver"]=resp.ver;
    j["req_id"]=resp.req_id;
    j["type"]=resp.type;
    j["ok"]=resp.ok;
    j["code"]=im::errCodeToString(resp.code);
    j["msg"]=resp.msg;
    j["data"]=resp.data;
    return j.dump();
}