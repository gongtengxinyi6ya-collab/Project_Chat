#pragma once
#include <string>
#include <vector>
/*
管理群信息和群成员关系*/
class GroupRepo{
public:
    virtual ~GroupRepo()=default;
    virtual bool createGroup(const std::string& groupId,const std::string& groupName,const std::string& owner)=0;//创建微信群聊
    virtual bool groupExists(const std::string& groupId)=0;//群聊是否存在
    virtual bool addMember(const std::string& groupId,const std::string& username)=0;//用户入群时保存群成员关系
    virtual bool removeMember(const std::string& groupId,const std::string& username)=0;//主动退群或群主踢人删除关系
    virtual std::vector<std::string> listMembers(const std::string& groupId)=0;//服务重建时恢复时可重建GroupManager

};