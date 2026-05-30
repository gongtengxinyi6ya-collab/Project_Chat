#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
storage::SqlGroupRepo::SqlGroupRepo(std::shared_ptr<SqlConnectionPool> pool)
:pool_(std::move(pool)){

}

storage::RepoResult storage::SqlGroupRepo::createGroup(const std::string& groupId,const std::string& groupName,const std::string&ownerAccountId){
    if(groupId.empty()||groupName.empty()||ownerAccountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO chat_groups(group_id,group_name,owner) VALUES(?,?,?)",{groupId,groupName,ownerAccountId});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        if(result.error.find("Duplicate entry")!=std::string::npos){
            return RepoResult{.status=RepoStatus::AlreadyExists,.message="Group already exists"};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to connect to database"};
}
bool storage::SqlGroupRepo::groupExists(const std::string& groupId){
    if(groupId.empty()){
        return false;
    }
    auto conn=pool_->acquire();
    if(!conn){
        return false;
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT id FROM chat_groups WHERE group_id=? LIMIT 1",{groupId});
        if(result.ok()&&!result.rows.empty()){
            return true;
        }
    }
    return false;
}

storage::RepoResult storage::SqlGroupRepo::addMember(const std::string&groupId,const std::string& accountId){
    if(groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="groupId is empty"};
    }
    if(accountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(!groupExists(groupId)){
        return RepoResult{.status=RepoStatus::NotFound,.message="Group not found"};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("INSERT INTO group_members(group_id,account_id) VALUES(?,?)",{groupId,accountId});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        if(result.error.find("Duplicate entry")!=std::string::npos){
            return RepoResult{.status=RepoStatus::AlreadyExists,.message="User is already a member of the group"};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to connect to database"};
}
storage::RepoResult storage::SqlGroupRepo::removeMember(const std::string& groupId,const std::string& accountId){
    if(groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="groupId is empty"};
    }
    if(accountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    if(!groupExists(groupId)){
        return RepoResult{.status=RepoStatus::NotFound,.message="Group not found"};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        auto result=conn->executePrepared("DELETE FROM group_members WHERE group_id=? AND account_id=?",{groupId,accountId});
        if(result.ok()){
            if(result.affectedRows>0){
                return RepoResult{.status=RepoStatus::Ok};
            }else{
                return RepoResult{.status=RepoStatus::NotFound,.message="User is not a member of the group"};
            }
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Failed to connect to database"};
}
std::vector<std::string> storage::SqlGroupRepo::listMembers(const std::string& groupId){
    if(groupId.empty()){
        return {};
    }
    auto conn=pool_->acquire();
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT account_id FROM group_members WHERE group_id=?",{groupId});
        if(result.ok()){
            std::vector<std::string> members;
            for(const auto& row:result.rows){
                auto accountIdIt=row.find("account_id");
                if(accountIdIt!=row.end()){
                    members.emplace_back(accountIdIt->second);
                }
            }
            return members;
        }
    }
    return {};
}
std::vector<storage::GroupRepo::GroupSnapshot> storage::SqlGroupRepo::listGroups(){
    auto conn=pool_->acquire();
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT group_id,group_name,owner FROM chat_groups",{});
        if(result.ok()){
            std::vector<GroupSnapshot> groupSnapshots;
            for(const auto& row:result.rows){
                GroupSnapshot groupSnapshot;
                auto idPair=row.find("group_id");
                groupSnapshot.groupId=idPair!=row.end()?idPair->second:"";
                auto namePair=row.find("group_name");
                groupSnapshot.groupName=namePair!=row.end()?namePair->second:"";
                auto ownerPair=row.find("owner");
                groupSnapshot.ownerAccountId=ownerPair!=row.end()?ownerPair->second:"";
                groupSnapshots.emplace_back(std::move(groupSnapshot));
            }
            return groupSnapshots;
        }
    }
    return {};
}

storage::RepoResult storage::SqlGroupRepo::createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId){
    if(groupId.empty()||groupName.empty()||ownerAccountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to accquire a conn"};
    }
    SqlTransaction tx(*conn);//开启事务
    try{
        //插入群
        auto result1=conn->executePrepared("INSERT INTO chat_groups(group_id,group_name,owner) VALUES(?,?,?)",{groupId,groupName,ownerAccountId});
        if(!result1.ok()&&result1.error.find("Duplicate entry")!=std::string::npos){
            return RepoResult{.status=RepoStatus::AlreadyExists,.message="Group already exists"};
        }
        else if(!result1.ok()){
            return RepoResult{.status=RepoStatus::SqlError,.message=result1.error};

        }
        //插入群主成员
        auto result2=conn->executePrepared("INSERT INTO group_members(group_id,account_id) VALUES(?,?)",{groupId,ownerAccountId});
        if(result1.ok()&&result2.ok()){
            tx.commit();
            return RepoResult{.status=RepoStatus::Ok};
        }
    }catch(const std::exception& e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Unknown error"};
}