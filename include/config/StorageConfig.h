#pragma once
#include <string>
#include "ConfigParseHelper.h"
#include "third_party/json.hpp"

/*存储后端选择相关配置*/

class StorageConfig{
public:
    static StorageConfig fromJson(const nlohmann::json& j);
    void loadFromEnv();
    void validateOrThrow()const;

    const std::string& type()const;
    bool fallbackToMemory()const;
private:
    //属性
    std::string type_{"memory"};//存储后端类型
    bool fallbackToMemory_{true};//SQL初始化失败是否退回内存存储
};