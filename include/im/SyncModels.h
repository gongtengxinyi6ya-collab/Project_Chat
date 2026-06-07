#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/ConversationRepo.h"
#include "storage/OfflineMessageRepo.h"
#include "third_party/json.hpp"
namespace im{
struct SyncCursor{//客户端某个会话的本地同步游标
    storage::ConversationType type;
    std::string targetId{};
    uint64_t lastMsgId{0};//客户端本地该会话最后一条消息ID
    size_t limit{50};//限制增量
};

struct ConversationDelta{//表示会话增量消息
    storage::ConversationType type;
    std::string targetId;//会话目标
    nlohmann::json messages;//新消息数组
};
struct SyncResult{//表示一次同步请求的聚合结果
    std::vector<ConversationDelta> deltas;//新增消息
    std::vector<storage::OfflineMessageIndex> offlineIndexes;//离线消息索引
};
}