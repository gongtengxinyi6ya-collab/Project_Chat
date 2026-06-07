#include <string>
namespace im{
inline std::string buildDirectConversationKey(const std::string& accountA,const std::string& accountB){
    if(accountA.empty()||accountB.empty()){
        return "";
    }
    if(accountA<accountB){
        return accountA+"#"+accountB;
    }
    else{
        return accountB+"#"+accountA;
    }
}
}