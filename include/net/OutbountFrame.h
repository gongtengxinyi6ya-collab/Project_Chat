#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include "net/SendTypes.h"

namespace net{
/*保存已经构造好的四字节长度头，共享JSON payload*/
class OutboundFrame{
public:
    static std::shared_ptr<const OutboundFrame> create(SharedPayload payload, std::size_t maxFrameLen);//静态工厂函数
    const char* headerData() const noexcept{return header_.data();};//返回长度头首地址。
    std::size_t headerBytes() const noexcept{return header_.size();};//固定返回 4。
    const std::string& payload() const noexcept{return *payload_;};//返回共享 JSON。
    std::size_t payloadBytes() const noexcept{return payload_->size();};//返回 JSON 长度。
    std::size_t frameBytes() const noexcept{return frameBytes_;};//放回4+payloadBytes()
private:
    std::array<char,4> header_{};//网路字节序长度头
    SharedPayload payload_;//共享JSON数据
    std::size_t frameBytes_{0};//完整帧长度

    OutboundFrame(SharedPayload payload, std::array<char, 4> header);//构造函数
};
}