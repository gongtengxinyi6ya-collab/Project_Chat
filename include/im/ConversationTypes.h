#pragma once
#include <string>
#include "storage/types/ConversationTypes.h"
namespace im{
struct ConversationView
    {//会话列表
        storage::ConversationSummary summary;
        std::string targetUsername{};//用户名
        std::string targetNickname{};//用户昵称
        std::string targetAvatarUrl{};//用户头像url
        std::string groupName{};//群聊名称
        std::string groupOwnerAccountId{};
    };

}