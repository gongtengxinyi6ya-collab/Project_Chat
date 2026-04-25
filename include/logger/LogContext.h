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

    bool empty()const;//判断上下文字段是否全空
    std::string toKvString() const;//把上下文转换为key=value格式的字符串，便于日志输出
};