#include "infra/health/HealthFormatter.h"
#include "storage/sql/SqlPoolStats.h"
#include <sstream>
namespace infra::health{
    std::string formatHealthSnapshot(const HealthSnapshot& snapshot){
    std::ostringstream oss;

    oss << "status=" << healthStatusToString(snapshot.status)
        << ", sqlEnabled=" << snapshot.sqlEnabled
        << ", sqlHealthy=" << snapshot.sqlHealthy
        <<",SqlStats: "<<storage::formatSqlPoolStats(snapshot.sqlStats)
        << ", redisEnabled=" << snapshot.redisEnabled
        << ", redisHealthy=" << snapshot.redisHealthy
        << ", onlineConnections=" << snapshot.onlineConnections
        << ", uptimeMs=" << snapshot.uptimeMs
        << ", logDropped=" << snapshot.logDropped
        << ", logWritten=" << snapshot.logWritten;

    if (!snapshot.reason.empty()) {
        oss << ", reason=" << snapshot.reason;
    }

    return oss.str();
}
}