#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
namespace storage{

enum class GroupStatus:uint8_t{//群生命周期状态
    Active=0,
    Dissolved=1
};

struct GroupSnapshot
{
    std::string groupId{};
    std::string groupName{};
    std::string ownerAccountId{};
    GroupStatus status{GroupStatus::Active};
    int64_t dissolvedAtMs{0};
};

struct GroupMemberRecord {
    std::string groupId{};
    std::string accountId{};
    uint8_t role{0};
    int64_t joinedAtMs{0};
};

struct GroupDissolveRecord {
    bool changed{false};
    bool alreadyDissolved{false};
    std::vector<std::string> memberAccountIds;
};

enum class GroupJoinRequestStatus : uint8_t {//入群申请状态
    Pending = 0,//待处理
    Approved = 1,//同意
    Rejected = 2//拒绝
};
struct GroupJoinRequestRecord{//保存一次入群申请的完整持久化状态
    uint64_t requestId{0};//申请id
    std::string groupId{};//申请群id
    std::string applicantAccountId{};//申请账号
    GroupJoinRequestStatus status{GroupJoinRequestStatus::Pending};
    std::string requestMessage{};//申请信息
    std::string reviewerAccountId{};//审核人账号
    int64_t createdAtMs{0};//创建时间
    int64_t reviewedAtMs{0};
};
struct GroupJoinApplyResult {//入群申请结果
    bool submitted{false};//本次是否新提交或重新提交
    bool alreadyPending{false};//是否已有待处理申请
    bool alreadyIn{false};//是否已经是群成员
    std::string groupId{};//群id
    std::string applicantAccountId{};//申请人账号
};
struct GroupJoinReviewResult {//入群审核结果
    bool approved{false};//审核是否同意
    bool rejected{false};//审核是否拒绝
    bool memberAdded{false};//是否真正新增群成员
    bool alreadyHandled{false};//申请是否已经被处理
    std::string groupId{};
    std::string applicantAccountId{};
};

struct GroupSearchResult {
    std::string groupId{};
    std::string groupName{};
    std::string ownerAccountId{};
    size_t memberCount{0};
    bool alreadyMember{false};
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

inline std::optional<GroupJoinRequestStatus> getGroupJoinRequestStatus(uint64_t value){
    switch(value){
        case 0:
            return GroupJoinRequestStatus::Pending;
        case 1:
            return GroupJoinRequestStatus::Approved;
        case 2:
            return GroupJoinRequestStatus::Rejected;
        default:
            return std::nullopt;
    }
}
}