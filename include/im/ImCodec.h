#include <string>
#include <variant>
#include <string_view>
#include <cstdint>
#include "ImMessage.h"

/*
*/
namespace im{   
 std::variant<Request,Response> tryParse(std::string_view payload);//把字符串解析为Request或Response,如果解析失败返回错误信息
 std::string encodeResponse(const Response& resp);//把Response编码为字符串

 //ReSponse辅助函数
 im::Response makeErr(const im::Request& req,im::ErrorCode code,const std::string& msg,nlohmann::json data=nlohmann::json{});
 im::Response makeOk(const im::Request& req,im::MsgType type,nlohmann::json data=nlohmann::json{},std::string mag="Ok");
}
