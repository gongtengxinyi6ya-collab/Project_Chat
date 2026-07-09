#include "config/MaintenanceConfig.h"
#include "config/ConfigParseHelper.h"
#include <exception>
MaintenanceConfig MaintenanceConfig::fromJson(const nlohmann::json& j){
    MaintenanceConfig config;
    config.enabled=ConfigParseHelper::getOrDefault(j,"enabled",config.enabled);
    config.intervalMs=ConfigParseHelper::getOrDefault(j,"interval_ms",config.intervalMs);
    config.expiredSessionRetentionMs=ConfigParseHelper::getOrDefault(j,"expired_session_retention_ms",config.expiredSessionRetentionMs);
    config.revokedSessionRetentionMs=ConfigParseHelper::getOrDefault(j,"revoked_session_retention_ms",config.revokedSessionRetentionMs);
    config.handledRequestRetentionMs=ConfigParseHelper::getOrDefault(j,"handled_request_retention_ms",config.handledRequestRetentionMs);
    config.offlineIndexRetentionMs=ConfigParseHelper::getOrDefault(j,"offline_index_retention_ms",config.offlineIndexRetentionMs);
    config.batchSize=ConfigParseHelper::getOrDefault(j,"batch_size",config.batchSize);
    
    return config;
}

void MaintenanceConfig::validateOrThrow()const{
    constexpr int64_t SECOND = 1000LL;
    constexpr int64_t MINUTE = 60LL * SECOND;
    constexpr int64_t HOUR   = 60LL * MINUTE;
    constexpr int64_t DAY    = 24LL * HOUR;

    ConfigParseHelper::checkRange("interval_ms",intervalMs,0,24LL*HOUR);
    ConfigParseHelper::checkRange("expired_session_retention_ms",expiredSessionRetentionMs,1LL*HOUR,90LL*DAY);
    ConfigParseHelper::checkRange("revoked_session_retention_ms",revokedSessionRetentionMs,1LL*DAY,180LL*DAY);
    ConfigParseHelper::checkRange("handled_request_retention_ms",handledRequestRetentionMs,1LL*DAY,180LL*DAY);
    ConfigParseHelper::checkRange("offline_index_retention_ms",offlineIndexRetentionMs,1LL*DAY,180LL*DAY);
    if (batchSize < 1 || batchSize > 10000) {
            throw std::runtime_error(
                "maintenance.batchSize out of range: " +
                std::to_string(batchSize) +
                ", expected [1, 10000]");
        }
}