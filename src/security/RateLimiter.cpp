#include "security/RateLimiter.h"
#include "security/RateLimitStore.h"
#include <stdexcept>

security::RateLimiter::RateLimiter(std::shared_ptr<RateLimitStore> store)
:store_(std::move(store)),
loginFailRule_{.name="login_fail",.maxRequests=5,.windowMs=5*60*1000,.blockMs=5*60*1000},
registerRule_{.name="register",.maxRequests=10,.windowMs=60*1000,.blockMs=5*60*1000},
sendMessageRule_{.name="send_message",.maxRequests=20,.windowMs=1000,.blockMs=3000},
syncRule_{.name="sync",.maxRequests=5,.windowMs=1000,.blockMs=3000},
historyRule_{.name="history",.maxRequests=10,.windowMs=1000,.blockMs=3000}
{
    if(!store_){
        throw std::invalid_argument("RateLimitStore invalid");
    }
}

security::RateLimitResult security::RateLimiter::checkRegister(const std::string& ip,int64_t nowMs){
    if(!store_||ip.empty()){
        return {.allowed=true};
    }
    return store_->hit("ip:"+ip,registerRule_,nowMs);
}

security::RateLimitResult security::RateLimiter::checkLoginFail(const std::string& accountId, int64_t nowMs){
    if(!store_||accountId.empty()){
        return {.allowed=true};
    }
    return store_->hit("account:"+accountId,loginFailRule_,nowMs);
}

void security::RateLimiter::resetLoginFail(const std::string& accountId){
    if(!store_||accountId.empty()){
        return;
    }
    store_->reset("account:"+accountId,loginFailRule_);
}

security::RateLimitResult security::RateLimiter::checkSendMessage(const std::string& accountId, int64_t nowMs){
    if(!store_||accountId.empty()){
        return {.allowed=true};
    }
    return store_->hit("account:"+accountId,sendMessageRule_,nowMs);
}

security::RateLimitResult security::RateLimiter::checkSync(const std::string& accountId, int64_t nowMs){
    if(!store_||accountId.empty()){
        return {.allowed=true};
    }
    return store_->hit("account:"+accountId,syncRule_,nowMs);
}

security::RateLimitResult security::RateLimiter::checkHistory(const std::string& accountId, int64_t nowMs){
    if(!store_||accountId.empty()){
        return {.allowed=true};
    }
    return store_->hit("account:"+accountId,historyRule_,nowMs);
}