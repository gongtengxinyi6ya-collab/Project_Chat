#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "storage/RepoValueResult.h"

namespace im {

template<typename T>
struct AsyncDbResult {//异步查询公共结果
    storage::RepoValueResult<T> repoResult{};//Repo查询结果

    std::int64_t queueWaitUs{0};//工作线程排队时间
    std::int64_t executeUs{0};//SQL执行时间
    std::string exceptionMessage{};//异常信息

    bool ok() const noexcept {
        return repoResult.ok() &&
               repoResult.value.has_value();
    }
};

}