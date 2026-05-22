#include <openssl/sha.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include "security/PasswordHasher.h"

security::PasswordHasher::PasswordHasher(size_t saltLength,const std::string& algorithm)
:saltLength_(saltLength),algorithm_(algorithm){

}

security::PasswordHash security::PasswordHasher::hashPassword(const std::string& password){
    if(password.empty()){
        throw std::invalid_argument("password is empty");
    }
    std::string salt=generateSalt();
    std::string hash=calculateHash(password,salt);
    return PasswordHash{.hash=hash,.salt=salt};
}
bool security::PasswordHasher::verifyPassword(const std::string& password,const std::string& expectedHash,const std::string&salt){
    if(password.empty()||expectedHash.empty()||salt.empty()){
        return false;
    }
    std::string hash=calculateHash(password,salt);
    return hash==expectedHash;
}
std::string security::PasswordHasher::generateSalt(){
    std::vector<unsigned char> buf(saltLength_);
    if(RAND_bytes(buf.data(),buf.size())!=1){
        //生成len字节密码学安全随机数
        throw std::runtime_error("RAND_bytes failed");
    }
    //将二进制数据转换为十六进制字符串
    std::stringstream ss;
    for(auto b:buf){
        ss
        <<std::hex//后续整数按16进制输出
        <<static_cast<int>(b)//转化为整数
        <<std::setw(2)//输出宽度至少为2
        <<std::setfill('0');//宽度不足补充0
    }
    return ss.str();
}
std::string security::PasswordHasher::calculateHash(const std::string& password,const std::string&salt){
    if(password.empty()||salt.empty()){
        return "";
    }
    const std::string input=password+salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),input.size(),hash);
    //将二进制哈希值转换为十六进制字符串
    std::stringstream ss;
    for(int i=0;i<SHA256_DIGEST_LENGTH;++i){
        ss
        <<std::hex
        <<std::setw(2)
        <<std::setfill('0')
        <<static_cast<int>(hash[i]);
    }
    return ss.str();
}