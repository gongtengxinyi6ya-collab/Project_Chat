#include <sw/redis++/redis++.h>
#include <chrono>
#include <stdexcept>
#include "logger/LogMacros.h"
#include "infra/redis/RedisClient.h"

namespace infra::redis{
struct RedisClient::Impl{
    RedisConfig config;//redis连接配置
    std::unique_ptr<sw::redis::Redis> redis;//执行redis对象
    bool connected{false};//客户端是否成功初始化并ping通过
};

RedisClient::RedisClient(const RedisConfig&config)
:impl_(std::make_unique<Impl>()){
    impl_->config=config;
}

RedisClient::~RedisClient()=default;
bool RedisClient::connect(){
    if(!impl_->config.enabled()){
        return false;
    }
    try{
        //构造连接
        sw::redis::ConnectionOptions connectionOpts;
        connectionOpts.host=impl_->config.host();
        connectionOpts.port=impl_->config.port();
        connectionOpts.password=impl_->config.password();
        connectionOpts.db=impl_->config.db();
        connectionOpts.connect_timeout=std::chrono::milliseconds(impl_->config.connectTimeoutMs());
        connectionOpts.socket_timeout=std::chrono::milliseconds(impl_->config.socketTimeoutMs());
        //构造连接池
        sw::redis::ConnectionPoolOptions poolOpts;
        poolOpts.size=impl_->config.poolSize();
        impl_->redis=std::make_unique<sw::redis::Redis>(connectionOpts,poolOpts);
        if(ping()){
            impl_->connected=true;
            return true;
        }
    }catch(const std::exception& e){
        impl_->redis.reset();
        impl_->connected=false;
        LOG_WARN(std::string("Faile to connect redis:")+e.what());
        return false;
    }
}

bool RedisClient::connected()const{
    if(!impl_){
        return false;
    }
    return impl_->connected;
}
bool RedisClient::ping(){
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        return impl_->redis->ping()=="PONG";

    }catch(const std::exception& e){
        impl_->connected=false;
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}

//基础命令

std::optional<std::string> RedisClient::get(const std::string& key){
    if(key.empty()){
        return std::nullopt;
    }
    if(!impl_||!impl_->redis){
        return std::nullopt;
    }
    try{
        auto opt=impl_->redis->get(key);
        if(!opt){
            return std::nullopt;
        }
        return opt.value();
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return std::nullopt;
    }
}

bool RedisClient::set(const std::string& key,const std::string&value){
    if(key.empty()){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
    return impl_->redis->set(key,value);
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
bool RedisClient::setEx(const std::string& key,const std::string&value,int64_t ttlSeconds){
    if(key.empty()||ttlSeconds<=0){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        impl_->redis->setex(key,ttlSeconds,value);
        return true;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
bool RedisClient::setPx(const std::string& key,const std::string&value,int64_t ttlMs){
    if(key.empty()||ttlMs<=0){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        impl_->redis->set(key,value,std::chrono::milliseconds(ttlMs));
        return true;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
bool RedisClient::del(const std::string& key){
    if(key.empty()){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        if(impl_->redis->del(key)>=0){
            return true;
        }
        return false;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
std::optional<int64_t> RedisClient::incr(const std::string& key){
    if(key.empty()){
        return std::nullopt;
    }
    if(!impl_||!impl_->redis){
        return std::nullopt;
    }
    try{
        auto incrValue=impl_->redis->incr(key);
        return incrValue;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return std::nullopt;
    }
}
bool RedisClient::expire(const std::string& key,int64_t ttlSeconds){
    if(key.empty()||ttlSeconds<=0){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        return impl_->redis->expire(key,std::chrono::seconds(ttlSeconds));
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
bool RedisClient::pexpire(const std::string& key,int64_t ttlMs){
    if(key.empty()||ttlMs<=0){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        return impl_->redis->pexpire(key,std::chrono::milliseconds(ttlMs));
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
int64_t RedisClient::pttl(const std::string& key){
    if(key.empty()){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        auto pttlValue=impl_->redis->pttl(key);
        if(pttlValue<0){
            return 0;
        }
        return pttlValue;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
bool RedisClient::exists(const std::string& key){
    if(key.empty()){
        return false;
    }
    if(!impl_||!impl_->redis){
        return false;
    }
    try{
        return impl_->redis->exists(key)>0;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return false;
    }
}
std::optional<int64_t> RedisClient::evalInt(const std::string& script,const std::vector<std::string>&keys,const std::vector<std::string>&args){
    if(script.empty()){
        return std::nullopt;
    }
    if(!impl_||!impl_->redis){
        return std::nullopt;
    }
    try{
        auto evalValue=impl_->redis->eval<int64_t>(script,keys.begin(),keys.end(),args.begin(),args.end());
        return evalValue;
    }catch(const std::exception& e){
        LOG_WARN(std::string("Redis command failed: ") + e.what());
        return std::nullopt;
    }
}
}