#pragma once
#include <cstdint>
#include "third_party/json.hpp"
#include "ConfigParseHelper.h"
/*校验heartBeatMs,maxFrameLen在合理区*/

class NetConfig{
public:
    static NetConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;

    //属性
    uint32_t heartBeatMs{50000};//心跳间隔，单位毫秒
    uint32_t heartbeatTimeoutMs{120000};//心跳超时时间，单位毫秒，建议设置为heartbeatInterval的2-3倍
    uint32_t maxFrameLen{65536};//最大帧长度，单位字节
    uint32_t connHighWaterMark{1*1024*1024};//连接背压高水位，单位字节，超过则记录过载日志但不强制断开连接
    uint32_t connLowWaterMark{512*1024};//连接背压低水位，单位字节，恢复正常状态的阈值，建议设置为高水位的一半
    uint32_t connHardLimit{10*1024*1024};//连接背压硬限制，单位字节，超过则强制断开连接，建议设置为高水位的10倍
    uint32_t maxOverloadDropCount{1000};//过载丢弃次数上限，超过该次数可以考虑关闭连接或触发更严重的限流措施
    bool tcpNoDelay{true};
    bool keepAlive{true};
};