#include <cstdint>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>
#include <unordered_map>
#include "storage/MessageRepo.h"
/*内存储存实现，后续可替换为数据库实现*/
namespace storage{
class MemoryMessageRepo:public storage::MessageRepo{
public:
struct MessageRecord{
    uint64_t messageId;
    std::string groupId;
    std::string from;
    std::string content;
    uint64_t serverTsMs;
};
    storage::SaveMessageResult saveGroupMessage(uint64_t,const std::string& groupId,const std::string&from,const std::string&content,uint64_t serverTsMs)override;//保存群消息
private:
    std::unordered_map<std::string,std::vector<MessageRecord>> groupMessages_;//groupId映射消息列表
    mutable std::mutex mutex_;//保护groupMessages_的读写
};
}
