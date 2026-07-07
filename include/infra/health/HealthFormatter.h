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
std::string formatHealthSnapshot(const HealthSnapshot& snapshot);
}