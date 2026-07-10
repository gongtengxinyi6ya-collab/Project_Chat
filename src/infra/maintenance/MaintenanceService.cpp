#include "infra/maintenance/MaintenanceService.h"
#include "storage/UserSessionRepo.h"
#include "storage/FriendRequestRepo.h"
#include "storage/GroupJoinRequestRepo.h"
#include "storage/OfflineMessageRepo.h"

#include <chrono>
#include <string_view>
#include <exception>
#include <string>

namespace infra::maintenance{

MaintenanceService::MaintenanceService(MaintenanceConfig config, storage::RepositoryBundle repos)
:config_(config),repos_(repos){

}

int64_t MaintenanceService::nowMs()const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

MaintenanceSnapshot MaintenanceService::snapshot() const{
    std::lock_guard lk(snapshotMutex_);
    auto snapshot=snapshot_;
    snapshot.running=running_.load(std::memory_order_acquire);
    return snapshot;
}
MaintenanceStats MaintenanceService::runOnce(){
    bool expected=false;
    if(!running_.compare_exchange_strong(expected,true)){
        return {.ok=true};
    }
    MaintenanceStats stats;
    stats.startedAtMs=nowMs();

    struct RunningGrard{//保护running_函数执行完后置为false
        std::atomic<bool>& running;
        ~RunningGrard(){
            running.store(false,std::memory_order_release);
        }
    }guard{running_};

    bool success=true;
    auto runTask=[&](std::string_view name,auto&& fn)->size_t{
        try{
            return fn();
        }catch(const std::exception& e){
            if(!name.empty()){
                stats.error+="; ";
            }
            success=false;
            stats.error+=std::string(name)+" failed:"+e.what();
            return 0;
        }catch(...){
            if(!name.empty()){
                stats.error+="; ";
            }
            success=false;
            stats.error+=std::string(name)+" failed: unknown exception";
            return 0;
        }
    };

    stats.expiredSessionsDeleted=runTask("cleanupExpiredSessions",[&]{
        return cleanupExpiredSessions(stats.startedAtMs);
    });
    stats.revokedSessionsDeleted=runTask("cleanupRevokedSessions",[&]{
        return cleanupRevokedSessions(stats.startedAtMs);
    });
    stats.friendRequestsDeleted=runTask("cleanupHandledFriendRequests",[&]{
        return cleanupHandledFriendRequests(stats.startedAtMs);
    });
    stats.groupJoinRequestsDeleted=runTask("cleanupHandledGroupJoinRequests",[&]{
        return cleanupHandledGroupJoinRequests(stats.startedAtMs);
    });
    stats.offlineIndexesDeleted=runTask("cleanupOfflineIndexes",[&]{
        return cleanupOfflineIndexes(stats.startedAtMs);
    });
    
    stats.finishedAtMs=nowMs();
    lastRunAtMs_.store(stats.finishedAtMs,std::memory_order_relaxed);
    if(success){
        lastSuccessAtMs_.store(stats.finishedAtMs,std::memory_order_relaxed);
    }
    stats.ok=success;
    {
        std::lock_guard lk(snapshotMutex_);
        snapshot_.running=false;
        snapshot_.lastRunOk=stats.ok;
        snapshot_.totalRuns++;
        if(stats.ok){
            snapshot_.successRuns++;
            snapshot_.lastSuccessAtMs=stats.finishedAtMs;
        }
        else{
            snapshot_.failedRuns++;
        }
        snapshot_.lastRunAtMs=stats.startedAtMs;
        snapshot_.lastDurationMs=stats.startedAtMs=stats.finishedAtMs;
        snapshot_.lastDeleted=stats.totalDeleted();
        snapshot_.lastError=stats.error;
    }
    return stats;
}

    size_t MaintenanceService::cleanupExpiredSessions(int64_t nowMs){
        if(!repos_.userSessionRepo){
            return 0;
        }
        auto cutoff=nowMs-config_.expiredSessionRetentionMs;
        auto maxBatchesPerRun=config_.maxBatchesPerRun;
        size_t totalclean;
        while(maxBatchesPerRun){
        auto result=repos_.userSessionRepo->deleteExpiredBefore(cutoff,config_.batchSize);
        if(!result.ok()){
            throw std::runtime_error(result.message);
        }
        if(!result.value.has_value()){
            throw std::runtime_error("returned no value");
        }
        totalclean+=result.value.value();
        maxBatchesPerRun--;
        if(result.value.value()<config_.batchSize){
            break;
        }
    }
        return totalclean;

        
    }
    size_t MaintenanceService::cleanupRevokedSessions(int64_t nowMs){
        if(!repos_.userSessionRepo){
            return 0;
        }
        auto cutoff=nowMs-config_.revokedSessionRetentionMs;
        auto maxBatchesPerRun=config_.maxBatchesPerRun;
        size_t totalclean;
        while(maxBatchesPerRun){
        auto result=repos_.userSessionRepo->deleteRevokedBefore(cutoff,config_.batchSize);
        if(!result.ok()){
            throw std::runtime_error(result.message);
        }
        if(!result.value.has_value()){
            throw std::runtime_error("returned no value");
        }
        totalclean+=result.value.value();
        maxBatchesPerRun--;
        if(result.value.value()<config_.batchSize){
            break;
        }
    }
        return totalclean;
}
    size_t MaintenanceService::cleanupHandledFriendRequests(int64_t nowMs){
        if(!repos_.friendRequestRepo){
            return 0;
        }
        auto cutoff=nowMs-config_.handledRequestRetentionMs;
        auto maxBatchesPerRun=config_.maxBatchesPerRun;
        size_t totalclean;
        while(maxBatchesPerRun){
        auto result=repos_.friendRequestRepo->deleteHandledBefore(cutoff,config_.batchSize);
        if(!result.ok()){
            throw std::runtime_error(result.message);
        }
        if(!result.value.has_value()){
            throw std::runtime_error("returned no value");
        }
        totalclean+=result.value.value();
        maxBatchesPerRun--;
        if(result.value.value()<config_.batchSize){
            break;
        }
    }
        return totalclean;
}
    size_t MaintenanceService::cleanupHandledGroupJoinRequests(int64_t nowMs){
        if(!repos_.groupJoinRequestRepo){
            return 0;
        }
        auto cutoff=nowMs-config_.handledRequestRetentionMs;
        auto maxBatchesPerRun=config_.maxBatchesPerRun;
        size_t totalclean;
        while(maxBatchesPerRun){
        auto result=repos_.groupJoinRequestRepo->deleteHandledBefore(cutoff,config_.batchSize);
        if(!result.ok()){
            throw std::runtime_error(result.message);
        }
        if(!result.value.has_value()){
            throw std::runtime_error("returned no value");
        }
        totalclean+=result.value.value();
        maxBatchesPerRun--;
        if(result.value.value()<config_.batchSize){
            break;
        }
    }
        return totalclean;
    }
    size_t MaintenanceService::cleanupOfflineIndexes(int64_t nowMs){
        if(!repos_.offlineMessageRepo){
            return 0;
        }
        auto cutoff=nowMs-config_.offlineIndexRetentionMs;
        auto maxBatchesPerRun=config_.maxBatchesPerRun;
        size_t totalclean;
        while(maxBatchesPerRun){
        auto result=repos_.offlineMessageRepo->deleteCreatedBefore(cutoff,config_.batchSize);
        if(!result.ok()){
            throw std::runtime_error(result.message);
        }
        if(!result.value.has_value()){
            throw std::runtime_error("returned no value");
        }
        totalclean+=result.value.value();
        maxBatchesPerRun--;
        if(result.value.value()<config_.batchSize){
            break;
        }
    }
        return totalclean;
}