#pragma once
#include <cstdint>
#include <cstddef>
/*表示日志系统运行状态*/

struct LoggerStats {
    bool asyncEnabled{false};
    bool asyncRunning{false};
    uint64_t written{0};
    uint64_t dropped{0};
    size_t queueSize{0};
};