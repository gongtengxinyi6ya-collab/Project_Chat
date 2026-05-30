#pragma once
#include <string>
#include <vector>
#include "storage/RepoResult.h"
/*
管理群信息和群成员关系*/
namespace storage{
class GroupRepo{
public:
    struct GroupSnapshot
    {
        std::string groupId;
        std::string groupName;
        std::string ownerAccountId;
    };

    virtual ~GroupRepo()=default;
    virtual RepoResult createGroup(const std::string& groupId,const std::string& groupName,const std::string& ownerAccountId)=0;//创建微信群聊
    virtual bool groupExists(const std::string& groupId)=0;//群聊是否存在
    virtual RepoResult addMember(const std::string& groupId,const std::string& accountId)=0;//用户入群时保存群成员关系
    virtual RepoResult removeMember(const std::string& groupId,const std::string& accountId)=0;//主动退群或群主踢人删除关系
    virtual std::vector<std::string> listMembers(const std::string& groupId)=0;//服务重建时恢复时可重建GroupManager
    virtual std::vector<GroupSnapshot> listGroups()=0;//启动时读取所有群基础信息
    virtual RepoResult createGroupWithOwner(const std::string& groupId,const std::string& groupName,const std::string& accountId);//创建群基础信息，同时把owner加入群列表

};
}