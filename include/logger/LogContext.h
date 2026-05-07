#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <string_view>
#include "im/ErrorCode.h"
#include "im/MsgType.h"

/*
日志结构化*/
class LogContext{
public:
    std::optional<int> connFd;//连接fd
    std::optional<std::string> user;//用户名
    std::optional<std::string> groupId;//群ID
    std::optional<uint64_t> msgId;//服务端消息ID
    std::optional<uint64_t> reqId;//客户端请求ID
    std::optional<uint32_t> msgType;//消息类型
    std::optional<uint32_t> errCode;//错误码
    std::optional<std::string> event;//事件名
    std::optional<size_t> fanout;//广播扇出数
    std::optional<size_t> sent;//成功发送数量
    std::optional<size_t> dropped;//丢弃数，主要用于异步日志
    std::optional<size_t> closed;//目标连接已关闭数量
    std::optional<size_t> overloaded;//目标连接过载数量
    std::optional<size_t> noSuchConnection;//目标连接未存在
    std::optional<size_t> pendingBytes;//当前连接待发送缓冲区大小
    std::optional<std::string> sendResult;//发送结果
    bool empty()const;//判断上下文字段是否全空
    std::string toKvString() const;//把上下文转换为key=value格式的字符串，便于日志输出
};