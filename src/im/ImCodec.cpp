#include "im/ImCodec.h"
namespace im{
std::variant<Request,Response> tryParse(std::string_view payload){
    try{
        //JSON parse
        auto j=nlohmann::json::parse(payload);
        //校验ver/type/req_id
        if(!j.contains("ver")||!j.contains("type")||!j.contains("req_id")){
            return Response{.ver=1,.req_id=0,.type=MsgType::ERR,.ok=false,.code=ErrorCode::MISSING_FIELD,.msg="Missing required field",.data=nlohmann::json{}};
        }
        uint32_t ver=j["ver"].get<uint32_t>();
        if(ver!=1){
            return Response{.ver=ver,.req_id=0,.type=MsgType::ERR,.ok=false,.code=ErrorCode::UNSUPPORTED_VER,.msg="Unsupported protocol version",.data=nlohmann::json{}};
        }
        uint32_t type_int=j["type"].get<uint32_t>();
        auto type_opt=msgTypeFromInt(type_int);
        if (!type_opt.has_value()){
            return Response{.ver=ver,.req_id=0,.type=MsgType::ERR,.ok=false,.code=ErrorCode::UNKNOWN_TYPE,.msg="Unknown message type",.data=nlohmann::json{}};
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
        return Response{.ver=1,.req_id=0,.type=MsgType::ERR,.ok=false,.code=ErrorCode::BAD_JSON,.msg="Invalid JSON format",.data=nlohmann::json{}};
    }
}
//
std::string encodeResponse(const Response& resp){
    nlohmann::json j;
    j["ver"]=resp.ver;
    j["req_id"]=resp.req_id;
    j["type"]=msgTypeToInt(resp.type);
    j["ok"]=resp.ok;
    j["code"]=static_cast<uint16_t>(resp.code);
    j["msg"]=resp.msg;
    j["data"]=resp.data;
    return j.dump();
}

SyncParseResult parseSyncCursors(const Request& req,size_t defaultLimit,size_t maxLimit){
    SyncParseResult result{.ok=false};
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
        if(cursor.limit>maxLimit){
            cursor.limit=maxLimit;
        }
        result.cursors.emplace_back(std::move(cursor));
    }
    result.ok=true;
    return result;
}
//Response辅助函数
Response makeErr(const Request& req,ErrorCode code,const std::string& msg,nlohmann::json data){
    return Response{.ver=req.ver,.req_id=req.req_id,.type=MsgType::ERR,.ok=false,.code=code,.msg=msg,.data=data};
}
Response makeOk(const Request& req,MsgType type,nlohmann::json data,std::string mag){
    return Response{.ver=req.ver,.req_id=req.req_id,.type=type,.ok=true,.code=ErrorCode::OK,.msg=mag,.data=data};
}
size_t parseLimit(const Request& req,const std::string& key,size_t defaultValue,size_t limitValue){
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
std::optional<Response> parseUint64ArrayField(const Request&req,const std::string&field,std::vector<uint64_t>& out,size_t maxBatchSize){
    if (!req.body.contains(field)) {
        return std::nullopt;
    }
    const auto& arr = req.body[field];
    if (!arr.is_array()) {
        return makeErr(req,ErrorCode::BAD_REQUEST,"Field is not an array: "+field);
    }
    if (arr.empty()) {
        return makeErr(req,ErrorCode::BAD_REQUEST,"Array is empty: "+field);
    }
    if (arr.size() > maxBatchSize) {
        return makeErr(req,ErrorCode::ACK_BATCH_TOO_LARGE,"Batch size is too large: "+field);
    }
    out.clear();
    out.reserve(arr.size());
    for (const auto& item : arr) {
        uint64_t value = 0;
        if (item.is_number_unsigned()&&item.get<uint64_t>()>0) {
            value = item.get<uint64_t>();
        }
        else if (item.is_number_integer() && item.get<int64_t>() > 0) {
            value = static_cast<uint64_t>(item.get<int64_t>());
        }
        else {
            return makeErr(req,ErrorCode::BAD_REQUEST,"Invalid value for field: "+field);
        }
        out.push_back(value);
    }
    return std::nullopt;
}


MessageAckParseResult parseMessageAck(const Request& req,size_t maxBatchSize){
    MessageAckParseResult result;

    auto msgIdsResult = parseUint64ArrayField(req,"msgIds",result.payload.msgIds,maxBatchSize);
    if(msgIdsResult){
        return {.ok=false,.code=msgIdsResult->code,.message=msgIdsResult->msg};
    }
    auto offlineIdsResult = parseUint64ArrayField(req,"offlineMsgIds",result.payload.offlineMsgIds,maxBatchSize);
    if(offlineIdsResult){
        return {.ok=false,.code=offlineIdsResult->code,.message=offlineIdsResult->msg};
    }
    if(result.payload.msgIds.empty()&&result.payload.offlineMsgIds.empty()){
        return {.ok=false,.code=ErrorCode::INVALID_ACK_PAYLOAD,.message="msgIds and offlineMsgIds cannot both be empty"};
    }
    result.ok=true;
    return result;
}

HistoryQueryParseResult parseHistoryQuery(const Request&req,size_t defaultLimit,size_t maxLimit){
    size_t limit=0;
    if (req.body.contains("limit")) {//存在字段
        if (req.body["limit"].is_number_unsigned()) {
            limit = req.body["limit"].get<size_t>();
        } 
        else if (req.body["limit"].is_number_integer() &&req.body["limit"].get<int64_t>() > 0)
        {
            limit =static_cast<size_t>(req.body["limit"].get<int64_t>());
        } 
        else{//存在但不是非负整数
            return {.code=ErrorCode::BAD_REQUEST};
        }
    }
    else{//不存在取默认值
        limit=defaultLimit;
    }
    if(limit>maxLimit){//超过限制截断
        limit=maxLimit;
    }

    //读取beforeMsgId
    uint64_t beforeMsgId=0;
    if (req.body.contains("beforeMsgId")) {//存在字段
        if (req.body["beforeMsgId"].is_number_unsigned()) {
            beforeMsgId = req.body["beforeMsgId"].get<size_t>();
        } 
        else if (req.body["beforeMsgId"].is_number_integer() &&req.body["beforeMsgId"].get<int64_t>() > 0)
        {
            beforeMsgId =static_cast<size_t>(req.body["beforeMsgId"].get<int64_t>());
        } 
        else{//存在但不是非负整数
            return {.code=ErrorCode::BAD_REQUEST};
        }
    }
    else{
        beforeMsgId=0;
    }
    //读取lastMsgId
    uint64_t lastMsgId=0;
    if (req.body.contains("lastMsgId")) {//存在字段
        if (req.body["lastMsgId"].is_number_unsigned()) {
            lastMsgId = req.body["lastMsgId"].get<size_t>();
        } 
        else if (req.body["lastMsgId"].is_number_integer() &&req.body["lastMsgId"].get<int64_t>() > 0)
        {
            lastMsgId =static_cast<size_t>(req.body["lastMsgId"].get<int64_t>());
        } 
        else{//存在但不是非负整数
            return {.code=ErrorCode::BAD_REQUEST};
        }
    }
    //互斥校验
    if(beforeMsgId>0&&lastMsgId>0){
        return {.code=ErrorCode::BAD_REQUEST};
    }
    HistoryQueryMode mode{};
    //模式判断
    if(beforeMsgId==0&&lastMsgId==0){
        mode=HistoryQueryMode::Latest;
    }
    else if(beforeMsgId>0&&lastMsgId==0){
        mode=HistoryQueryMode::Before;
    }
    else if(beforeMsgId==0&&lastMsgId>0){
        mode=HistoryQueryMode::After;
    }
    HistoryQuery query{.mode=mode,.beforeMsgId=beforeMsgId,.lastMsgId=lastMsgId,.limit=limit};
    return {.ok=true,.query=query};
}

DispatchResult DispatchResult::immediate(Response response){
    return DispatchResult{.mode=DispatchMode::Immediate,.response=std::move(response)};
}
DispatchResult DispatchResult::deferred(){
    return {.mode=DispatchMode::Deferred,.response=std::nullopt};
}
bool DispatchResult::shouldRespond()const noexcept{
    return mode==DispatchMode::Immediate&&response.has_value();
}
}