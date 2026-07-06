#pragma once
#include <string>
#include "HealthSnapshot.h"

namespace infra::health{
inline std::string healthStatusToString(HealthStatus status){
    switch(status){
        case HealthStatus::Healthy:
            return "healthy";
        case HealthStatus::Degraded:
            return "degraded";
        case HealthStatus::Unhealthy:
            return "unhealthy";   
    }
}
inline std::string formatHealthSnapshot(const HealthSnapshot& snapshot){
    return "health: status="+healthStatusToString(snapshot.status)+" uptimeMs="+std::to_string(snapshot.uptimeMs)+" online="+std::to_string(snapshot.onlineConnections)+" sqlStarted=";
}
}