#pragma once
#include <string>
#include <optional>
#include "storage/RepoResult.h"
/*存储结果辅助类，可携带查询或写入产生的数据
*/
namespace storage{
    template<typename T>
struct RepoValueResult{

    RepoStatus status{RepoStatus::Internal};//表达成功、已存在、未找到或SQL错误
    std::string message{};//错误信息
    std::optional<T> value{std::nullopt};//查询结果或写入结果等

    bool ok()const{
        return status==RepoStatus::Ok;}
};
}