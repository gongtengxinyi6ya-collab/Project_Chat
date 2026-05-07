#include <string>
/*
管理用户持久化接口*/
class UserRepo{
public:
    virtual ~UserRepo()=default;
    virtual bool createUser(const std::string& username)=0;//注册或者首次登录时创建用户
    virtual bool userExissts(const std::string& username)=0;//判断用户是否存在
};