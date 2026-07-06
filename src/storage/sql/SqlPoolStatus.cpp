#include "storage/sql/SqlPoolStats.h"
namespace storage{
    std::string formatSqlPoolStats(const SqlConnectionPoolStats&stats){
        return "sql_pool total=" + std::to_string(stats.total) +
       " idle=" + std::to_string(stats.idle) +
       " inUse=" + std::to_string(stats.inUse) +
       " acquireCount=" + std::to_string(stats.acquireCount) +
       " acquireTimeouts=" + std::to_string(stats.acquireTimeouts) +
       " reconnects=" + std::to_string(stats.reconnects) +
       " replaceFailures=" + std::to_string(stats.replaceFailures);
    }
}