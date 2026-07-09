#pragma once 
#include <cstdint>
#include "third_party/json.hpp"

/*后台维护任务配置*/
class MaintenanceConfig{
public:
    static MaintenanceConfig fromJson(const nlohmann::json& j);
    void validateOrThrow() const;

    bool enabled{true};//是否开启维护任务
    int64_t intervalMs{600000};//清理间隔
    int64_t expiredSessionRetentionMs{7LL * 24 * 3600 * 1000};//token过期后保留时间
    int64_t revokedSessionRetentionMs{7LL * 24 * 3600 * 1000};//用户退出登录后session保留时间
    int64_t handledRequestRetentionMs{30LL * 24 * 3600 * 1000};//申请保留时间
    int64_t offlineIndexRetentionMs{30LL * 24 * 3600 * 1000};//离线消息索引保留时间
    size_t batchSize{500};//每次清除行数
};
