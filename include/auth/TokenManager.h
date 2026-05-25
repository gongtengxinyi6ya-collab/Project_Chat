#pragma once
#include <cstdint>
#include <string>

/*负责生成安全随机token
把原始token转成token hash
验证token格式和过期时间辅助信息*/
namespace auth{
struct IssuedToken{
    std::string rawToken;//返回给客户端，仅返回一次
    std::string tokenHash;//保存到数据库
    int64_t expireAtMs;//过期时间
};
class TokenManager{
public:
    IssuedToken issueToken();//登录成功后生成新的登录凭证
    std::string hashToken(const std::string& rewToken)const;//客户端使用token重连时，把收到的原始token转为hash,用于数据库查询
private:
    size_t tokenBytes_{32};//原始随机token的字节长度
    int64_t expireSeconds_{7*24*3600};//token有效时间
    std::string sha256Hex(const std::string&)const;
};
}