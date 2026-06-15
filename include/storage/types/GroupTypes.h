#pragma once
#include <string>
#include <cstdint>
namespace storage{

struct GroupSnapshot
{
    std::string groupId;
    std::string groupName;
    std::string ownerAccountId;
    GroupStatus status{GroupStatus::Active};
    int64_t dissolvedAtMs{0};
};

struct GroupMemberRecord {
    std::string groupId;
    std::string accountId;
    uint8_t role{0};
    int64_t joinedAtMs{0};
};

enum class GroupStatus:uint8_t{//群生命周期状态
    Active=0,
    Dissolved=1
};

struct GroupDissolveRecord {
    bool changed{false};
    bool alreadyDissolved{false};
    std::vector<std::string> memberAccountIds;
};
}