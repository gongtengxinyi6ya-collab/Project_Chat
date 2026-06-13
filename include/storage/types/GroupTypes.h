#pragma once
#include <string>
#include <cstdint>
namespace storage{

struct GroupSnapshot
{
    std::string groupId;
    std::string groupName;
    std::string ownerAccountId;
};

struct GroupMemberRecord {
    std::string groupId;
    std::string accountId;
    uint8_t role{0};
    int64_t joinedAtMs{0};
};
}