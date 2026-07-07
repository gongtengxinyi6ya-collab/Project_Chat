#include "storage/sql/SqlPoolStats.h"
#include <sstream>
namespace storage{
    std::string formatSqlPoolStats(const SqlConnectionPoolStats&stats){
        std::ostringstream oss;
        oss
        <<"sql_pool started"<<stats.started
        <<"total=" <<stats.total
        <<" idle=" <<stats.idle
        <<" inUse=" <<stats.inUse
        <<" acquireCount=" <<stats.acquireCount
        <<"acquireTimeoutMs"<<stats.acquireTimeoutMs
        <<" acquireTimeouts=" <<stats.acquireTimeouts
        <<" reconnects=" <<stats.reconnects
        <<" replaceFailures=" <<stats.replaceFailures;
        return oss.str();
    }
}