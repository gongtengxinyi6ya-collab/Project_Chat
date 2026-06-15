#pragma once
#include <string>
#include <cstdint>
#include <optional>
namespace storage{

enum class GroupStatus:uint8_t{//群生命周期状态
    Active=0,
    Dissolved=1
};

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

struct GroupDissolveRecord {
    bool changed{false};
    bool alreadyDissolved{false};
    std::vector<std::string> memberAccountIds;
};

inline std::optional<GroupStatus> getGroupStatusFromUint(uint64_t value){
    switch(value){
        case 0:
            return GroupStatus::Active;
        case 1:
            return GroupStatus::Dissolved;
        default:
            return std::nullopt;
    }
}
}