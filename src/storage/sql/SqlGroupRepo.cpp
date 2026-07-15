#include "storage/sql/SqlGroupRepo.h"
#include "storage/sql/SqlConnectionPool.h"
#include "storage/sql/SqlConnectionGuard.h"
#include "storage/sql/SqlConnection.h"
#include "storage/sql/SqlTransaction.h"
#include "storage/sql/SqlErrorMapper.h"
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
        auto status=mapSqlErrorToRepoStatus(result);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="User already exiest"};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result)};
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
        auto result=conn->queryPrepared("SELECT id FROM chat_groups WHERE group_id=? AND status=0 LIMIT 1",{groupId});
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
    auto conn=pool_->acquire();
    if(!conn){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    if(conn->connected()){
        std::string sql=R"(
        INSERT INTO group_members(group_id, account_id, role)
        SELECT group_id, ?, ?
        FROM chat_groups
        WHERE group_id = ?
        AND status = 0;
        )";
        auto result=conn->executePrepared(sql,{accountId,static_cast<uint64_t>(role),groupId});
        if(result.ok()){
            return RepoResult{.status=RepoStatus::Ok};
        }
        auto status=mapSqlErrorToRepoStatus(result);
        if(status==RepoStatus::AlreadyExists){
            return {.status=RepoStatus::AlreadyExists,.message="User already exiest"};
        }
        return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result)};
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
    if(role>2){
        return RepoResult{.status=RepoStatus::InvalidGroupRole,.message="role is invaild"};
    }
    auto conn=pool_->acquire();
    if(!conn||!conn->connected()){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to acquire a conn"};
    }
    auto result=conn->executePrepared("UPDATE group_members SET role=? WHERE group_id=? AND account_id=?",{static_cast<uint64_t>(role),groupId,accountId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.affectedRows==0){
        if(isMember(groupId,accountId)){//重复设置幂等成功
            return {.status=RepoStatus::Ok};
        }
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
    if(!conn||!conn->connected()){
        return RepoResult{.status=RepoStatus::Internal,.message="Failed to acquire a conn"};
    }
    try{
        //开启事务
        SqlTransaction transation(*conn);
        //删除群成员
        auto result=conn->executePrepared("DELETE FROM group_members WHERE group_id=? AND account_id=?",{groupId,accountId});
        if(!result.ok()){
            return RepoResult{.status=RepoStatus::SqlError,.message=result.error};
        }
        if(result.affectedRows==0){
            return RepoResult{.status=RepoStatus::NotFound,.message=result.error};
        }
        //删除成员游标
        auto cursorResult=conn->executePrepared(R"(
            DELETE FROM user_group_cursors
            WHERE account_id = ?
            AND group_id = ?;
            )"
        ,{accountId,groupId});
        if(!cursorResult.ok()){
            return {.status=RepoStatus::SqlError,.message=cursorResult.error};
        }
        transation.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }
}
std::vector<storage::GroupMemberRecord> storage::SqlGroupRepo::listMemberRecords(const std::string& groupId){
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
    
    std::vector<GroupMemberRecord> members;
    for(const auto& row:result.rows){
       GroupMemberRecord member;
        member.accountId=getString(row,"account_id");
        member.groupId=getString(row,"group_id");
        member.role=static_cast<uint8_t>(getUInt64(row,"role"));
        member.joinedAtMs=getUInt64(row,"joined_at_ms");
        members.emplace_back(std::move(member));
    }
    return members;
}
std::vector<storage::GroupSnapshot> storage::SqlGroupRepo::listGroups(){
    auto conn=pool_->acquire();
    if(!conn){
        return {};
    }
    if(conn->connected()){
        auto result=conn->queryPrepared("SELECT group_id,group_name,owner,status,COALESCE(dissolved_at_ms, 0) AS dissolved_at_ms FROM chat_groups WHERE status=0",{});
        if(result.ok()){
            std::vector<GroupSnapshot> groupSnapshots;
            for(const auto& row:result.rows){
                GroupSnapshot snapShot;
                snapShot.groupId=getString(row,"group_id");
                snapShot.groupName=getString(row,"group_name");
                snapShot.ownerAccountId=getString(row,"owner");
                auto statusOpt=getGroupStatusFromUint(getUInt64(row,"status"));
                if(statusOpt.has_value()){
                    snapShot.status=statusOpt.value();
                }
                snapShot.dissolvedAtMs=getInt64(row,"dissolved_at_ms");
                groupSnapshots.emplace_back(std::move(snapShot));
            }
            return groupSnapshots;
        }
    }
    return {};
}
std::vector<storage::GroupSnapshot> storage::SqlGroupRepo::findGroupsByIds(const std::vector<std::string>& groupIds){
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
    auto result=conn->queryPrepared("SELECT group_id,group_name,owner FROM chat_groups WHERE group_id IN ("+placeholders+") AND status=0",params);
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
    if(!conn||!conn->connected()){
        return RepoResult{.status=RepoStatus::SqlError,.message="Failed to accquire a conn"};
    }

    try{
        SqlTransaction tx(*conn);//开启事务
        //插入群
        auto result1=conn->executePrepared("INSERT INTO chat_groups(group_id,group_name,owner) VALUES(?,?,?)",{groupId,groupName,ownerAccountId});
        if(!result1.ok()){
            auto status=mapSqlErrorToRepoStatus(result1);
            if(status==RepoStatus::AlreadyExists){
                return {.status=RepoStatus::AlreadyExists,.message="group already exiest"};
            }
            return RepoResult{.status=RepoStatus::SqlError,.message=formatSqlError(result1)};
        }
        
        //插入群主成员
        auto result2=conn->executePrepared("INSERT INTO group_members(group_id,account_id,role) VALUES(?,?,2)",{groupId,ownerAccountId});
        if(!result2.ok()){
            return {.status=RepoStatus::SqlError,.message=result2.error};
        }
        if(result2.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=result2.error};
        }
        //插入群会话头
        auto headResult=conn->executePrepared(R"(
            INSERT INTO group_conversation_heads (
                group_id,
                last_seq
            )
            VALUES (?, 0);
            )",
        {groupId});
        if(!headResult.ok()){
            return {.status=RepoStatus::SqlError,.message=headResult.error};
        }
        if(headResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=headResult.error};
        }
        //增加群主游标
        auto cursorResult=conn->executePrepared(R"(
            INSERT INTO user_group_cursors (
                account_id,
                group_id,
                last_read_seq,
                last_read_msg_id,
                last_read_at_ms,
                joined_seq
            )
            VALUES (?, ?, 0, 0, 0, 0);
            )",
        {ownerAccountId,groupId});
        if(!cursorResult.ok()){
            return {.status=RepoStatus::SqlError,.message=cursorResult.error};
        }
        if(cursorResult.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=cursorResult.error};
        }
        tx.commit();
        return {.status=RepoStatus::Ok};
    }catch(const std::exception& e){
        return RepoResult{.status=RepoStatus::SqlError,.message=e.what()};
    }
    return RepoResult{.status=RepoStatus::SqlError,.message="Unknown error"};
}

storage::RepoValueResult<storage::GroupSnapshot> storage::SqlGroupRepo::findGroupById(const std::string& groupId){
    if(groupId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::SqlError,.message="Failed to accquire a conn"};
    }
    std::string sql=R"(
    SELECT
        group_id,
        group_name,
        owner,
        status,
        COALESCE(dissolved_at_ms, 0) AS dissolved_at_ms
    FROM chat_groups
    WHERE group_id = ?
    LIMIT 1
    )";
    auto result=conn->queryPrepared(sql,{groupId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.rows.empty()){
        return {.status=RepoStatus::NotFound,.message=result.error};
    }
    const auto& row=result.rows.front();
    GroupSnapshot snapShot;
    snapShot.groupId=getString(row,"group_id");
    snapShot.groupName=getString(row,"group_name");
    snapShot.ownerAccountId=getString(row,"owner");
    auto statusOpt=getGroupStatusFromUint(getUInt64(row,"status"));
    if(!statusOpt.has_value()){
        return {.status=RepoStatus::Internal,.message="status invalid"};
    }
    snapShot.status=statusOpt.value();
    snapShot.dissolvedAtMs=getInt64(row,"dissolved_at_ms");
    return {.status=RepoStatus::Ok,.value=snapShot};

}
storage::RepoValueResult<size_t> storage::SqlGroupRepo::countMembers(const std::string& groupId){
    if(groupId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::SqlError,.message="Failed to accquire a conn"};
    }
    std::string sql=R"(
    SELECT COUNT(*) AS member_count
    FROM group_members
    WHERE group_id = ?;
    )";
    auto result=conn->queryPrepared(sql,{groupId});
    if(!result.ok()){
        return {.status=RepoStatus::SqlError,.message=result.error};
    }
    if(result.rows.empty()){
        return {.status=RepoStatus::NotFound,.message=result.error};
    }
    const auto&row=result.rows.front();
    size_t count=static_cast<size_t>(getUInt64(row,"member_count"));
    return {.status=RepoStatus::Ok,.value=count};
}
storage::RepoValueResult<storage::GroupDissolveRecord> storage::SqlGroupRepo::dissolveGroup(const std::string& groupId,const std::string& ownerAccountId,int64_t dissolvedAtMs){
    if(groupId.empty()||ownerAccountId.empty()){
        return {.status=RepoStatus::InvalidArgument};
    }
    auto conn=pool_->acquire();//获取连接
    if(!conn||!conn->connected()){
        return {.status=RepoStatus::SqlError,.message="Failed to accquire a conn"};
    }
    //开启事务
    try{
        SqlTransaction transation(*conn);
        //锁定群记录
        std::string sqlRecord=R"(
        SELECT owner, status
        FROM chat_groups
        WHERE group_id = ?
        FOR UPDATE;
        )";
        auto resultLockRecord=conn->queryPrepared(sqlRecord,{groupId});
        if(!resultLockRecord.ok()){
            return {.status=RepoStatus::SqlError,.message=resultLockRecord.error};
        }
        if(resultLockRecord.rows.empty()){
            return {.status=RepoStatus::NotFound,.message=resultLockRecord.error};
        }
        const auto& rowRecord=resultLockRecord.rows.front();
        if(getString(rowRecord,"owner")!=ownerAccountId){
            return {.status=RepoStatus::NoPermission,.message="only the owner can dissolveGroup"};
        }
        auto statusOpt=getGroupStatusFromUint(getUInt64(rowRecord,"status"));
        if(!statusOpt.has_value()){
            return {.status = RepoStatus::Internal,.message = "invalid group status"};
        }
        if(statusOpt.value()==GroupStatus::Dissolved){//已解散幂等成功
            return {.status=RepoStatus::Ok,.value=GroupDissolveRecord{.alreadyDissolved=true}};
        }
        //解散前查询成员
        auto resultMember=conn->queryPrepared("SELECT account_id FROM group_members WHERE group_id=?",{groupId});
        if(!resultMember.ok()){
            return {.status=RepoStatus::SqlError,.message=resultMember.error};
        }
        if(resultMember.rows.empty()){
            return {.status=RepoStatus::NotFound,.message=resultMember.error};
        }
        GroupDissolveRecord record;
        for(const auto& row:resultMember.rows){
            auto accountId=getString(row,"account_id");
            if(!accountId.empty()){
                record.memberAccountIds.push_back(accountId);
            }
        }
        //标记群解散
        std::string sql=R"(
        UPDATE chat_groups
        SET status = 1,
            dissolved_at_ms = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE group_id = ?
        AND owner = ?
        AND status = 0;
        )";
        auto resultDissolved=conn->executePrepared(sql,{dissolvedAtMs,groupId,ownerAccountId});
        if(!resultDissolved.ok()){
            return {.status=RepoStatus::SqlError,.message=resultDissolved.error};
        }
        if(resultDissolved.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=resultDissolved.error};
        }
        //删除成员关系
        auto resultDeleteMember=conn->executePrepared("DELETE FROM group_members WHERE group_id=?",{groupId});
        if(!resultDeleteMember.ok()){
            return {.status=RepoStatus::SqlError,.message=resultDeleteMember.error};
        }
        if(resultDeleteMember.affectedRows==0){
            return {.status=RepoStatus::NotFound,.message=resultDeleteMember.error};
        }
        //删除未投递群离线索引
        auto resultDeleteOfflineMsg=conn->executePrepared("DELETE FROM offline_messages WHERE msg_type=1 AND group_id=?",{groupId});
        if(!resultDeleteOfflineMsg.ok()){
            return {.status=RepoStatus::SqlError,.message=resultDeleteOfflineMsg.error};
        }
        //删除用户游标
        auto cursorResult=conn->executePrepared(R"(
            DELETE FROM user_group_cursors
                WHERE group_id = ?;
            )",{groupId});
        if(!cursorResult.ok()){
            return {.status=RepoStatus::SqlError,.message=cursorResult.error};
        }
        //删除群会话头
        auto headResult=conn->executePrepared(R"(
            DELETE FROM group_conversation_heads
            WHERE group_id = ?;
            )",{groupId});
        if(!headResult.ok()){
            return {.status=RepoStatus::SqlError,.message=headResult.error};
        }
        //提交事务
        transation.commit();
        record.changed=true;
        return {.status=RepoStatus::Ok,.value=record};
    }catch(const std::exception& e){
        return {.status=RepoStatus::Internal,.message=e.what()};
    }
}