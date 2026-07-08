#include "TcpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "infra/health/HealthService.h"
#include "infra/health/HealthFormatter.h"
#ifdef PROJECT_CHAT_ENABLE_REDIS
#include "infra/redis/RedisClient.h"
#include "security/rate_limit/RedisRateLimitStore.h"
#include "security/rate_limit/RateLimiter.h"
#endif
TcpServer::TcpServer(EventLoop* loop,int port,const AppConfig& config)
:baseloop_(loop),acceptor_(baseloop_,port),threadNum_(config.server().ioThreads),started_(false),config_(config){
    iothreadPool_ = std::make_unique<EventLoopThreadPool>(baseloop_);
    acceptor_.setNewConnectionCallback([this](int fd){
        newConnection(fd);
    });
    threadPool_ = std::make_unique<ThreadPool>();
    imService_ = std::make_unique<im::Imservice>(1,config_.im(),config_.id());
    imService_->setSendToConnKey([this](im::Imservice::ConnKey key,const std::string& payload){
            auto it=connections_.find(key);
            if(it==connections_.end()){
                return im::Imservice::SendResult::NoSuchConnection;
            }
            if(it->second->isClosed()){
                return im::Imservice::SendResult::Closed;
            }
            if(!it->second->canSend(payload.size())){
                it->second->recordDrop(payload.size());
                return im::Imservice::SendResult::Overloaded;
            }
            it->second->send(payload);
            return im::Imservice::SendResult::Ok;
});
    //创建healthService_
    healthService_=std::make_unique<infra::health::HealthService>();
    healthService_->setConfig(config_.health());
    auto repos=storage::RepositoryFactory::create(config_);
    if(repos.hasSqlPool()){
        healthService_->setSqlPool(repos.sqlPool);
    }
    imService_->setRepositories(std::move(repos));
    imService_->loadFromRepositories();
    //注入在线连接数provider
    healthService_->setOnlineConnectionProvider([this](){
        return connections_.size();
    });
    //
    #ifdef PROJECT_CHAT_ENABLE_REDIS
    if (config_.redis().enabled()) {
        redisClient_= std::make_shared<infra::redis::RedisClient>(config_.redis());
        if (redisClient_->connect()) {
            std::string prefix = config_.redis().keyPrefix();
            if (!prefix.empty() && prefix.back() != ':') {
                prefix.push_back(':');
            }
            prefix += "rate:";

            auto store = std::make_shared<security::RedisRateLimitStore>(redisClient_, prefix);
            imService_->setRateLimiter(std::make_unique<security::RateLimiter>(store));
            healthService_->setRedisClient(redisClient_);
            LOG_INFO("Redis rate limiter enabled");
        } 
        else {
            LOG_WARN("Redis connect failed, rate limiter disabled");
        }
    }
    #else
    if (config_.redis().enabled()) {
        LOG_WARN("Redis is enabled in config, but binary was built without PROJECT_CHAT_ENABLE_REDIS");
    }
    #endif
}

TcpServer::~TcpServer(){
    // Ensure remaining connections are cleaned up if server is destroyed.
    connections_.clear();
}

void TcpServer::start(){
    if(started_)
        return;
    started_=true;
    iothreadPool_->setThreadNum(threadNum_);
    iothreadPool_->start();
    acceptor_.listen();
    if(config_.health().enabled()){
        baseloop_->runEvery(std::chrono::milliseconds(config_.health().logIntervalMs()),[this](){
            auto snapshot=healthService_->snapshot();
            LOG_INFO(infra::health::formatHealthSnapshot(snapshot));
        });
    }
}
//从Acceptor接收到新连接，在EventLoopThreadPool中选择一个IO线程，创建TcpConnection对象，
//再投递到baseloop保存到connections_中
void TcpServer::newConnection(int fd){
    //检测fd重复
    if(connections_.find(fd)!=connections_.end()){
        LOG_ERROR("Duplicate fd "+std::to_string(fd)+" received in newConnection, closing it");
        ::close(fd);
        return;
    }
    EventLoop* ioloop=iothreadPool_->getNextLoop();
    ioloop->runInLoop([this,fd,ioloop](){
        auto conn=std::make_shared<TcpConnection>(ioloop,fd,threadPool_.get(),this,config_);
        conn->setMessageCallback([this](std::shared_ptr<TcpConnection> conn,const std::string& msg){
            baseloop_->runInLoop([this,conn,msg](){
                onMessage(conn,msg);
            });
        });
        conn->setCloseCallback([this](std::shared_ptr<TcpConnection> conn){
                baseloop_->runInLoop([this,conn](){
                    removeConnectionInBaseLoop(conn);
                });
        });
        conn->setHighWaterCallback([](const std::shared_ptr<TcpConnection>& conn,size_t pending){
            LOG_WARN("connection high water fd="+std::to_string(conn->fd())+" pending="+std::to_string(pending));
        });
        conn->setLowWaterCallback([](const std::shared_ptr<TcpConnection>& conn,size_t pending){
            LOG_INFO("connection low water fd="+std::to_string(conn->fd())+" pending="+std::to_string(pending));
        });
        baseloop_->runInLoop([this,conn](){
            addConnectionInBaseLoop(conn);
            conn->getLoop()->runInLoop([conn](){//确保connectionEstablished在IO线程中执行，注册事件 
                conn->connectionEstablished();
            });
        });
    });
    LOG_INFO("New connection fd: " + std::to_string(fd)+" assigned to ioloop" );
}

void TcpServer::removeConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    auto it=connections_.find(fd);
    if(it!=connections_.end()){
        auto ioloop=conn->getLoop();
        connections_.erase(it);
        LOG_INFO("Connection removed fd: " + std::to_string(fd));
        ioloop->queueInLoop([conn](){
            conn->connectionDestroyed();
        });
    }
    imService_->onDisconnect(conn);//通知IM业务连接断开，清理状态

}

void TcpServer::onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& msg){
    //转发给IM业务对象处理
    imService_->onMessage(conn,msg);    
}
void TcpServer::setThreadNum(int numThreads){
    threadNum_=numThreads;
}

void TcpServer::addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    connections_[fd]=std::move(conn);
}