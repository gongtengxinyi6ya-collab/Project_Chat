#include <unordered_map>
#include <unordered_set>
#include <string>

//房间管理类
namespace im{
class{
public:
    using ConnKey=int;


private:
    std::unordered_map<std::string,std::unordered_set<ConnKey>> roomMembers_;

};
}
