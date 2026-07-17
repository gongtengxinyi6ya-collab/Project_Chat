#include "net/OutboundFrame.h"
#include <arpa/inet.h>
#include <cstring>
namespace net{
std::shared_ptr<const OutboundFrame> OutboundFrame::create(SharedPayload payload,std::size_t maxFrameLen){
    if(!payload){
        return nullptr;
    }
    auto payloadSize=payload->size();
    if(payloadSize>maxFrameLen||payloadSize>UINT32_MAX){
        return nullptr;
    }
    //转换为网络字节序
    const std::uint32_t netLength=htonl(static_cast<std::uint32_t>(payloadSize));
    //将四字节写入header_
    std::array<char,4> header{};
    std::memcpy(header.data(),&netLength,sizeof(netLength));
    return std::shared_ptr<const OutboundFrame>(new OutboundFrame(std::move(payload),std::move(header)));
}

OutboundFrame::OutboundFrame(SharedPayload payload,std::array<char,4> header)
:header_(std::move(header)),payload_(std::move(payload)),frameBytes_(payload_->size()+header_.size()){
    
}
}