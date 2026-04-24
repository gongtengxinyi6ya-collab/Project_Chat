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
    uint32_t idleTimeoutMs{60000};//连接空闲超时时间，单位毫秒
    uint32_t maxFrameLen{65536};//最大帧长度，单位字节
};