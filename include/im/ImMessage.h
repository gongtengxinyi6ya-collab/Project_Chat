#pragma once
#include <string>
#include <variant>
#include "third_party/json.hpp"
#include "MsgType.h"
#include "ErrorCode.h"
/*
将std::string payload 解析为结构化Request
把Response编码为std::string
统一做字段校验，版本校验，type校验*/
//请求：客户端发给服务端
struct Request{
    uint32_t ver;//协议版本
    MsgType type;//请求类型
    uint64_t req_id;//请求id
    uint64_t seq;//消息序列号,每个连接上递增,用于请求响应匹配和消息重排
    std::string from;//消息来源,可以是用户id,设备id等,用于权限校验和日志记录
    std::string to;//消息目的,可以是用户id,设备id等,用于路由和权限校验
    nlohmann::json body;//保存整个JSON,业务层后面取name/content用
};
//相应：服务端返回客户端
struct Response{
    uint32_t ver;
    uint64_t req_id;
    bool ok;//请求是否成功
    ErrorCode code;//错误码
    std::string msg;//错误信息
    nlohmann::json data;//扩展返回字段
};
static std::variant<Request,Response> parseOrError(std::string_view payload);//把字符串解析为Request或Response,如果解析失败返回错误信息
    
static std::string encodeResponse(const Response& resp);//把Response编码为字符串