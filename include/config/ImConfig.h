#pragma once
#include <cstddef>
#include "third_party/json.hpp"
/*协议约束参数化，避免ImService里写死常量*/

class ImConfig{
public:
    static ImConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;
private:
    bool requireGroupIdForSend{true};
    size_t maxGroupNameLen{64};//群名称长度
    size_t maxMessageLen{4096};//消息最长长度
};