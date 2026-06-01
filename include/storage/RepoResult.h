#pragma once
#include <string>
/*统一表达储存层结果
避免bool无法区分已存在/不存在/SQL错误
后续映射到im::ErrorCode*/
namespace storage{
enum class RepoStatus{
    Ok,//成功
    AlreadyExists,//唯一键冲突
    NotFound,//查询不到
    InvalidArgument,//参数非法
    SqlError,//SQL执行异常
    Internal,//内部未知错误
};

class RepoResult{
public:
    RepoStatus status{RepoStatus::Ok};
    std::string message{};

    bool ok()const{return status==RepoStatus::Ok;}
};
}