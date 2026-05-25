#pragma once
#include <string>
/*
负责密码哈希和密码校验*/
namespace security{
//保存一次密码哈希的结果
struct PasswordHash{
    std::string hash;
    std::string salt;
};

class PasswordHasher{
public:
    explicit PasswordHasher(size_t saltLength=16,const std::string& alogorithm="sha256");
    PasswordHash hashPassword(const std::string& password);//用户注册时调用，输出salt+hash
    bool verifyPassword(const std::string& password,const std::string& expextedHash,const std::string& salt);//登录时调用，对用户密码进行计算后与数据库中的哈希密码进行校验
    std::string generateSalt();//生成随机salt
    std::string calculateHash(const std::string& password,const std::string& salt);//根据密码和salt计算哈希值
    
private:
    size_t saltLength_;//salt长度
    std::string algorithm_;//当前算法名称
};
}