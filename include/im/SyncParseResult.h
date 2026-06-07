#pragma once
#include <string>
#include <vector>
#include "im/ErrorCode.h"
#include "im/SyncModels.h"
namespace im{
    struct SynaParseResult{
        bool ok{false};//解析是否成功
        ErrorCode code{ErrorCode::OK};//返回协议错误码
        std::string message{};//错误信息
        std::vector<SyncCursor> cursors{};
    };
}