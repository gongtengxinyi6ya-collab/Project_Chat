#include <openssl/rand.h>
#include <stdexcept>
#include "auth/AccountIdGenerator.h"
#include <vector>
std::string auth::AccountIdGenerator::generateAccountId(const size_t totalLength){
    if(totalLength<6||totalLength>20){
        throw std::invalid_argument("accountId lenth must be 6-20");
    }
    size_t randomLen=totalLength-2;
    std::vector<unsigned char> buf(randomLen);
    if(RAND_bytes(buf.data(),buf.size())!=1){
        throw std::runtime_error("RAND_bytes failed");
    }
    static constexpr char kChars[]="0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string output="im";
    for(auto b:buf){
        output.push_back(kChars[b%sizeof(kChars)-1]);
    }
    return output;
}