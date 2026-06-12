#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
#include <unordered_set>
#include <stdexcept>
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
bool storage::SqlGroupRepo::isMember(const std::string& groupId,const std::string& accountId){
    if(groupId.empty()||accountId.empty()){
        return false;
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return false;
    }

    auto result=conn->queryPrepared("SELECT id FROM group_members WHERE group_id=? AND account_id=? LIMIT 1",{groupId,accountId});
    if(!result.ok()){
        return false;
    }
    if(result.rows.empty()){
        return false;
    }
    return true;
}

storage::RepoResult storage::SqlGroupRepo::addMember(const std::string&groupId,const std::string& accountId,uint8_t role){
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
        auto result=conn->executePrepared("INSERT INTO group_members(group_id,account_id,role) VALUES(?,?,?)",{groupId,accountId,static_cast<uint64_t>(role)});
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

storage::RepoValueResult<uint8_t> storage::SqlGroupRepo::getMemberRole(const std::string&groupId,const std::string& accountId){
    if(groupId.empty()||accountId.empty()){
        return {.status=RepoStatus::InvalidArgument,.message="invaild argument"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }
    auto result=conn->queryPrepared("SELECT role FROM group_members WHERE group_id=? AND account_id=? LIMIT 1",{groupId,accountId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.rows.empty()){
        return {.status=RepoStatus::NotFound,.message="member not found"};
    }
    auto role=getUInt64(result.rows.front(),"role");
    return {.status=RepoStatus::Ok,.value=static_cast<uint8_t>(role)};
}

storage::RepoResult storage::SqlGroupRepo::updateMemberRole(const std::string& groupId,const std::string& accountId,uint8_t role){
    if(groupId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="groupId is empty"};
    }
    if(accountId.empty()){
        return RepoResult{.status=RepoStatus::InvalidArgument,.message="accountId is empty"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    auto result=conn->executePrepared("UPDATE group_members SET role=? WHERE group_id=? AND account_id=?",{groupId,accountId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.affectedRows==0){
        return {.status=RepoStatus::NotFound,.message="not found"};
    }
    return {.status=RepoStatus::Ok};
}
storage::RepoResult storage::SqlGroupRepo::transferOwner(const std::string& groupId,const std::string& oldOwner,const std::string& newOwner){
    if(groupId.empty()||oldOwner.empty()||newOwner.empty()){
        return {.status=RepoStatus::InvalidArgument,.message="invaild argument"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::Internal,.message="Failed to connect the database"};
    }
    //开启事务：更新群群主，还需要同步更新群成员表中角色
    try{
        SqlTransaction transation(*conn);
        //更新chat_groups中群的群主
        auto result1=conn->executePrepared("UPDATE chat_groups SET owner=? WHERE group_id=? AND owner=?",{newOwner,groupId,oldOwner});
        if(!result1.ok()){
            return {.status=RepoStatus::SqlError,.message=result1.error};
        }
        if(result1.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message="Not found"};
        }
        //更新group_members 新群主角色
        auto result2=conn->executePrepared("UPDATE group_members SET role=2 WHERE group_id=? AND account_id=?",{groupId,newOwner});
        if(!result2.ok()){
            return {.status=RepoStatus::SqlError,.message=result2.error};
        }
        if(result2.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message="Not found"};
        }
        //更新旧群主角色
        auto result3=conn->executePrepared("UPDATE group_members SET role=0 WHERE group_id=? AND account_id=?",{groupId,oldOwner});
        if(!result3.ok()){
            return {.status=RepoStatus::SqlError,.message=result3.error};
        }
        if(result3.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message="Not found"};
        }
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
    
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
std::vector<storage::GroupRepo::GroupMemberRecord> storage::SqlGroupRepo::listMemberRecords(const std::string& groupId){
    if(groupId.empty()){
        return {};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    const std::string sql=R"(
    SELECT group_id, account_id, role, UNIX_TIMESTAMP(joined_at) * 1000 AS joined_at_ms
    FROM group_members
    WHERE group_id = ?
    ORDER BY role DESC, joined_at ASC;
    )";
    auto result=conn->queryPrepared(sql,{groupId});
    if(!result.ok()){
        return {};
    }
    
    std::vector<GroupRepo::GroupMemberRecord> members;
    for(const auto& row:result.rows){
        GroupRepo::GroupMemberRecord member;
        member.accountId=getString(row,"account_id");
        member.groupId=getString(row,"group_id");
        member.role=static_cast<uint8_t>(getUInt64(row,"role"));
        member.joinedAtMs=getUInt64(row,"joined_at_ms");
        members.emplace_back(std::move(member));
    }
    return members;
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
std::vector<storage::GroupRepo::GroupSnapshot> storage::SqlGroupRepo::findGroupsByIds(const std::vector<std::string>& groupIds){
    if(groupIds.empty()){
        return {};
    }
    //去重
    std::unordered_set<std::string> groupIdsSet;
    std::vector<SqlParam> params;
    for(const auto& groupId:groupIds){
        if(groupIdsSet.insert(groupId).second){
            params.emplace_back(groupId);
        }
    }
    if(params.empty()){
        return {};
    }
    //统计占位符数量
    std::string placeholders;
    placeholders.reserve(params.size()*2);
    for(size_t i=0;i<params.size();i++){
        if(i==0){
            placeholders+="?";
        }
        else{
            placeholders+=",?";
        }
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return {};
    }
    auto result=conn->queryPrepared("SELECT group_id,group_name,owner FROM chat_groups WHERE group_id IN ("+placeholders+")",params);
    if(!result.ok()){
        return {};
    }
    if(result.rows.empty()){
        return {};
    }
    std::vector<GroupSnapshot> groupSnapshots;
    for(const auto& row:result.rows){
        GroupSnapshot groupSnapshot;
        groupSnapshot.groupId=getString(row,"group_id");
        groupSnapshot.groupName=getString(row,"group_name");
        groupSnapshot.ownerAccountId=getString(row,"owner");
        groupSnapshots.emplace_back(std::move(groupSnapshot));
    }
    return groupSnapshots;
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