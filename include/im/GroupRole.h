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
}