#include "auth/TokenManager.h"
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <openssl/sha.h>
#include <chrono>
std::string auth::TokenManager::sha256Hex(const std::string& input)const{
    unsigned char hash[SHA224_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),input.size(),hash);
    std::ostringstream tokenHash;
    for(int i=0;i<SHA256_DIGEST_LENGTH;++i){
        tokenHash
        <<std::hex
        <<std::setw(2) 
        <<std::setfill('0')
        <<static_cast<int>(hash[i]);
    }
    return tokenHash.str();
}
auth::IssuedToken auth::TokenManager::issueToken(){
    //生成字节随机数
    std::vector<unsigned char> buf(tokenBytes_);
    if(RAND_bytes(buf.data(),static_cast<int>(buf.size()))!=1){
        throw std::runtime_error("RAND_bytes failed");
    }
    //转化为十六进制字符串
    std::ostringstream rawToken;
    for(unsigned char b:buf){
        rawToken
        <<std::hex
        <<std::setw(2)
        <<std::setfill('0')
        <<static_cast<int>(b);
    }
    //计算hash
    auto tokenHash=sha256Hex(rawToken.str());
    //计算过期时间
    int64_t expireAtMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()+expireSeconds_*1000;
    return IssuedToken{.rawToken=rawToken.str(),.tokenHash=tokenHash,.expireAtMs=expireAtMs};

}

std::string auth::TokenManager::hashToken(const std::string&rawToken)const{
    if(rawToken.empty()){
        return "";
    }
    return sha256Hex(rawToken);
}