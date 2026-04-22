#include <cstdint>
#include "third_party/json.hpp"

/*校验heartBeatMs,maxFrameLen在合理区*/

class NetConfig{
public:
    static NetConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;
private:
    uint32_t heartBeatMs{50000};//心跳间隔，单位毫秒
    uint32_t idleTimeoutMs{60000};//连接空闲超时时间，单位毫秒
    uint32_t maxFrameLen{65536};//最大帧长度，单位字节
};