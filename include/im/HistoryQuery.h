#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace im{
enum class HistoryQueryMode{
    Latest,//首次进入聊天页，拉取最近limit条
    Before,//上滑加载旧消息
    After//本地已有消息，拉取lastMsgId之后新消息
};

struct HistoryQuery {
    HistoryQueryMode mode{HistoryQueryMode::Latest};//本次历史查询模式
    uint64_t beforeMsgId{0};//向前翻页游标，查更旧新消息
    uint64_t lastMsgId{0};//增量补齐游标，查更新消息
    size_t limit{50};
};

inline std::string historyQueryModeToString(HistoryQueryMode mode){
    switch(mode){
        case HistoryQueryMode::Latest:
            return "latest";
        case HistoryQueryMode::Before:
            return "before";
        case HistoryQueryMode::After:
            return "after";
    }
    return "unknown";
}
}