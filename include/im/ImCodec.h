#pragma once
#include <string>
#include <variant>
#include <string_view>
#include <cstdint>
#include <vector>
#include <optional>
#include "ImMessage.h"
#include "SyncParseResult.h"
/*
*/
namespace im{   
    std::variant<Request,Response> tryParse(std::string_view payload);//把字符串解析为Request或Response,如果解析失败返回错误信息
    std::string encodeResponse(const Response& resp);//把Response编码为字符串
    SyncParseResult parseSyncCursors(const Request&req,size_t defaultLimit);//解析SYNC_REQ中cursors并把顶层limit下发给未单独设置limit的cursor
    size_t parseLimit(const Request& req,const std::string& key,size_t defaultValue,size_t limitValue);
    //ReSponse辅助函数
    im::Response makeErr(const im::Request& req,im::ErrorCode code,const std::string& msg,nlohmann::json data=nlohmann::json{});
    im::Response makeOk(const im::Request& req,im::MsgType type,nlohmann::json data=nlohmann::json{},std::string mag="Ok");

    struct MessageAckPayload {//客户端ACK请求中解析出来的数据
    std::vector<uint64_t> msgIds;
    std::vector<uint64_t> offlineMsgIds;
};
    struct MessageAckParseResult {//解析结果
    bool ok{false};
    ErrorCode code{ErrorCode::OK};
    std::string message;
    MessageAckPayload payload;
};
    MessageAckParseResult parseMessageAck(const Request& req,size_t maxBatchSize);//从JSON请求中解析msgIds和offlineIds
    std::optional<im::Response> parseUint64ArrayField(const Request&req,const std::string&field,std::vector<uint64_t>& out,size_t maxBatchSize);
}
