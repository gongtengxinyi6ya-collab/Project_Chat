#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "storage/types/ConversationTypes.h"
#include "storage/types/MessageTypes.h"
#include "third_party/json.hpp"
namespace im{
struct SyncCursor{//客户端某个会话的本地同步游标
    storage::ConversationType type{storage::ConversationType::Unknown};//Direct/Group
    std::string targetId{};//私聊：accountId,群聊：groupId
    uint64_t lastMsgId{0};//客户端本地该会话最后一条消息ID
    size_t limit{50};//限制增量
};

struct ConversationDelta{//表示会话增量消息
    storage::ConversationType type{storage::ConversationType::Unknown};
    std::string targetId{};//会话目标
    uint64_t fromMsgId{0};//客户端上传的lastMsgId
    uint64_t latestMsgId{0};//本次返回消息的最大msgId
    bool hasMore{false};//如果返回条数达到limit，提示客户端可能还要更多需要继续拉取
    nlohmann::json messages{};//新消息数组
};
struct SyncResult{//表示一次同步请求的聚合结果
    std::vector<ConversationDelta> deltas;//新增消息
    std::vector<storage::OfflineMessageIndex> offlineIndexes;//离线消息索引
};
}