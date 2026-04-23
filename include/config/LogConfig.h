#pragma once
#include <string>
#include <set>
#include "third_party/json.hpp"
#include "ConfigParseHelper.h"
/*决定输出目标和格式*/
class LogConfig{
public:
    static LogConfig fromJson(const nlohmann::json&);
    void applyEnvOverrides();
    void validateOrThrow() const;

    //属性
    std::string level{"INFO"};
    bool toConsole{true};
    bool toFile{false};
    std::string filePath{"build/chat.log"};
    bool jsonFormat{false};
};