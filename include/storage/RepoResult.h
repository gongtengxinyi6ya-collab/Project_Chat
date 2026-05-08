#pragma once
#include <string>
/*统一表达储存层结果
避免bool无法区分已存在/不存在/SQL错误
后续映射到im::ErrorCode*/
namespace storage{
enum class RepoStatus{
    Ok,
    AlreadyExists,
    NotFound,
    InvalidArgument,
    SqlError
};

class RepoResult{
public:
    RepoStatus status{RepoStatus::Ok};
    std::string message;

    bool ok()const{return status==RepoStatus::Ok;}
};
}