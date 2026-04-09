#include "im/ImCodec.h"

std::variant<Request,Response> tryParse(std::string_view payload){
    try{
        //先处理控制帧
        if(payload=="ping"){
            return Request{.ver=1,.type=MsgType::ECHO_REQ,.req_id=0,.seq=0,.from="",.to="",.body=nlohmann::json{}};
        }
        if(payload=="pong"){
            return Request{.ver=1,.type=MsgType::ECHO_RESP,.req_id=0,.seq=0,.from="",.to="",.body=nlohmann::json{}};
        }
        //JSON parse
        auto j=nlohmann::json::parse(payload);
        //校验ver/type/req_id
        if(!j.contains("ver")||!j.contains("type")||!j.contains("req_id")){
            return Response{.ver=1,.req_id=0,.ok=false,.code=ErrorCode::MISSING_FIELD,.msg="Missing required field",.data=nlohmann::json{}};
        }
        uint32_t ver=j["ver"].get<uint32_t>();
        if(ver!=1){
            return Response{.ver=ver,.req_id=0,.ok=false,.code=ErrorCode::UNSUPPORTED_VER,.msg="Unsupported protocol version",.data=nlohmann::json{}};
        }
        uint32_t type_int=j["type"].get<uint32_t>();
        auto type_opt=msgTypeFromInt(type_int);
        if (!type_opt.has_value()){
            return Response{.ver=ver,.req_id=0,.ok=false,.code=ErrorCode::UNKNOWN_TYPE,.msg="Unknown message type",.data=nlohmann::json{}};
        }
        MsgType type=type_opt.value();
        uint64_t req_id=j["req_id"].get<uint64_t>();
        //构造Request
        Request req{.ver=ver,.type=type,.req_id=req_id,.seq=0,.from="",.to="",.body=j};
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
        return Response{.ver=1,.req_id=0,.ok=false,.code=ErrorCode::BAD_JSON,.msg="Invalid JSON format",.data=nlohmann::json{}};
    }
}

std::string encodeResponse(const Response& resp){
    nlohmann::json j;
    j["ver"]=resp.ver;
    j["req_id"]=resp.req_id;
    j["ok"]=resp.ok;
    j["code"]=errCodeToString(resp.code);
    j["msg"]=resp.msg;
    j["data"]=resp.data;
    return j.dump();
}