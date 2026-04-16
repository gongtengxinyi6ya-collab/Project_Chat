#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <optional>

//房间管理类
namespace im{
class RoomManager{
public:
    using ConnKey=int;
    bool join(const std::string &,ConnKey);//加入房间
    bool leave(const std::string&,ConnKey);//离开房间
    std::vector<ConnKey> members(const std::string &) const;//获取成员
    size_t memberCount(const std::string& )const ;//快速统计房间人数
    void removeKeyEverywhere(ConnKey,std::optional<std::string> knowRoom=std::nullopt);//断连时清理
    bool hasRoom(const std::string& room)const;//判断房间是否存在
    bool isMember(const std::string& room,ConnKey key) const;//检验成员资格
private:
    std::unordered_map<std::string,std::unordered_set<ConnKey>> roomMembers_;

};
}
