#pragma once 
#include <cstdint>
/*
群成员角色*/
namespace im{
    enum class GroupRole:uint8_t{
        Member=0,
        Admin=1,//管理员
        Owner=2//群主，最高权限
    };
    inline std::string roleToString(GroupRole role){
        switch(role){
            case GroupRole::Member:
                return "member";
            case GroupRole::Admin:
                return "admin";
            case GroupRole::Owner:
                return "owner";
            default:
            return "member";
        }
    }
}