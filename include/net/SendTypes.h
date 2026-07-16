#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <stddef.h>
namespace net{
using ConnKey=std::uint64_t;//连接标识类型

using SharedPayload=std::shared_ptr<const std::string>;//多个连接共享同一编码后的JSON

//SendResult:区分连接存在，输出缓冲区过载，成功入队
enum class SendResult{
    Ok,//成功入队
    NoSuchConnection,//连接不存在
    Closed,//连接关闭
    Overloaded,
    Failed//输出缓冲区过载
};

struct BatchSendResult{
    size_t sent{0};//成功发送数量
    size_t noSuchConnection{0};//没有连接数量
    size_t closed{0};//连接已关闭数量
    size_t overloaded{0};//输出缓冲区过载数量
    size_t failed{0};
    void add(SendResult result) noexcept{
        switch(result){
            case SendResult::Ok:
                sent++;
                return;
            case SendResult::NoSuchConnection:
                noSuchConnection++;
                return;
            case SendResult::Closed:
                closed++;
                return;
            case SendResult::Overloaded:
                overloaded++;
                return;
            case SendResult::Failed:
                failed++;
                return;
            default:
                return;
        }
    };//根据发送结果累计相应字段。
    std::size_t dropped() const noexcept{return noSuchConnection+closed+overloaded+failed;};//返回 noSuchConnection + closed + overloaded。
    std::size_t total() const noexcept{sent+dropped();};//返回 sent + dropped()。
    SendResult singleResult() const noexcept {
        if (sent > 0) {
            return SendResult::Ok;
        }
        if (overloaded > 0) {
            return SendResult::Overloaded;
        }
        if (closed > 0) {
            return SendResult::Closed;
        }
        if (noSuchConnection > 0) {
            return SendResult::NoSuchConnection;
        }
        return SendResult::Failed;
    }
};


}