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
//
std::string im::encodeResponse(const im::Response& resp){
    nlohmann::json j;
    j["ver"]=resp.ver;
    j["req_id"]=resp.req_id;
    j["type"]=im::msgTypeToInt(resp.type);
    j["ok"]=resp.ok;
    j["code"]=static_cast<uint16_t>(resp.code);
    j["msg"]=resp.msg;
    j["data"]=resp.data;
    return j.dump();
}

im::SyncParseResult im::parseSyncCursors(const Request& req,size_t defaultLimit){
    im::SyncParseResult result{.ok=false};
    if(!req.body.contains("cursors")){
        return {.ok=true};
    }
    if(!req.body["cursors"].is_array()){
        return {.ok=false,.code=ErrorCode::BAD_REQUEST,.message="cursors must be array"};
    }
    for(const auto& item:req.body["cursors"]){
        if(!item.is_object()){
            return {.code=ErrorCode::BAD_REQUEST,.message="cursor must be object"};
        }
        SyncCursor cursor;
        if(!item.contains("conversationType")){
            return {.code=ErrorCode::MISSING_FIELD,.message="missing conversationType"};
        }
        if(!item["conversationType"].is_string()){
            return {.code=ErrorCode::BAD_REQUEST,.message="conversation is not string"};
        }
        auto typeStr=item["conversationType"].get<std::string>();
        if(typeStr=="direct"){
            cursor.type=storage::ConversationType::Direct;
        }
        else if(typeStr=="group"){
            cursor.type=storage::ConversationType::Group;
        }
        else{
            return {.code=ErrorCode::BAD_REQUEST,.message="conversation must be direct or group"};
        }

        if (!item.contains("targetId")) {
            return {.code=ErrorCode::MISSING_FIELD,.message="missing targetId"};
        }
        if(!item["targetId"].is_string()){
            return {.code=ErrorCode::BAD_REQUEST,.message="targetId is not string"};
        }
        cursor.targetId = item["targetId"].get<std::string>();
        if (cursor.targetId.empty()) {
            return {.code=ErrorCode::BAD_REQUEST,.message="targetId is empty"};
        }

        // lastMsgId
        if (item.contains("lastMsgId")) {
            if (item["lastMsgId"].is_number_unsigned()) {
                cursor.lastMsgId = item["lastMsgId"].get<uint64_t>();
            } else if (item["lastMsgId"].is_number_integer() &&
                       item["lastMsgId"].get<int64_t>() >= 0) {
                cursor.lastMsgId =
                    static_cast<uint64_t>(item["lastMsgId"].get<int64_t>());
            } else {
                return {.code=ErrorCode::BAD_REQUEST,.message="lastMsgId is invalid"};
            }
        }
        else{
            cursor.lastMsgId=0;
        }

        // limit
        if (item.contains("limit")) {
            if (item["limit"].is_number_unsigned()) {
                cursor.limit = item["limit"].get<size_t>();
            } 
            else if (item["limit"].is_number_integer() &&item["limit"].get<int64_t>() > 0)
            {
                cursor.limit =static_cast<size_t>(item["limit"].get<int64_t>());
            } 
            else{
                return {.code=ErrorCode::BAD_REQUEST};
            }
        }
        else{
            cursor.limit=defaultLimit;
        }
        if(cursor.limit==0){
            cursor.limit=defaultLimit;
        }
        if(cursor.limit>100){
            cursor.limit=100;
        }
        result.cursors.emplace_back(std::move(cursor));
    }
    result.ok=true;
    return result;
}
//Response辅助函数
im::Response im::makeErr(const im::Request& req,im::ErrorCode code,const std::string& msg,nlohmann::json data){
    return im::Response{.ver=req.ver,.req_id=req.req_id,.type=im::MsgType::ERR,.ok=false,.code=code,.msg=msg,.data=data};
}
im::Response im::makeOk(const im::Request& req,im::MsgType type,nlohmann::json data,std::string mag){
    return im::Response{.ver=req.ver,.req_id=req.req_id,.type=type,.ok=true,.code=im::ErrorCode::OK,.msg=mag,.data=data};
}
std::vector<uint64_t> im::parseUint64ArrayField(const im::Request&req,const std::string&field,size_t maxBatchSize){
    if (!req.body.contains(field)) {
        return {};
    }
    const auto& arr = req.body[field];
    if (!arr.is_array()) {
        return {};
    }
    if (arr.empty()) {
        return {};
    }
    if (arr.size() > maxBatchSize) {
        return {};
    }
    std::vector<uint64_t> out;
    out.reserve(arr.size());
    for (const auto& item : arr) {
        uint64_t value = 0;
        if (item.is_number_unsigned()) {
            value = item.get<uint64_t>();
        }
        else if (item.is_number_integer() && item.get<int64_t>() > 0) {
            value = static_cast<uint64_t>(item.get<int64_t>());
        }
        else {
            return {};
        }
        out.push_back(value);
    }
    return out;
}
size_t im::parseLimit(const Request& req,const std::string& key,size_t defaultValue,size_t limitValue){
    if(req.body.contains(key)){
        size_t result=defaultValue;
        if(req.body[key].is_number_unsigned()){
            result=req.body[key].get<size_t>();
        }
        else if(req.body[key].is_number_integer()&&req.body[key].get<int64_t>()>0){
            result=static_cast<size_t>(req.body[key].get<int64_t>());
        }
        if(result>limitValue){
            result=limitValue;
        }
        return result;
    }
    return defaultValue;
}

im::MessageAckParseResult im::parseMessageAck(const Request& req,size_t maxBatchSize){
    MessageAckParseResult result;
    result.payload.msgIds=parseUint64ArrayField(req,"msgIds",maxBatchSize);
    result.payload.offlineIds=parseUint64ArrayField(req,"offlineIds",maxBatchSize);
    if(result.payload.msgIds.empty()&&result.payload.offlineIds.empty()){
        return {.ok=false,.code=ErrorCode::BAD_REQUEST};
    }
    result.ok=true;
    return result;
}