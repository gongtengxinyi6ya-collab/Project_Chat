#pragma once
#include <string>
/*负责生成唯一候选账号ID*/
namespace auth{
class AccountIdGenerator{
public:
    static std::string generateAccountId(const size_t totalLenth=10);
};
}