#include "TcpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "im/ImService.h"
#include "logger/LogMacros.h"
#include "infra/health/HealthService.h"
#include "infra/health/HealthFormatter.h"
#include "infra/maintenance/MaintenanceService.h"
#include "infra/thread/ThreadPool.h"
#ifdef PROJECT_CHAT_ENABLE_REDIS
#include "infra/redis/RedisClient.h"
#include "security/rate_limit/RedisRateLimitStore.h"
#include "security/rate_limit/RateLimiter.h"
#endif
#include <vector>
TcpServer::TcpServer(EventLoop* loop,int port,const AppConfig& config)
:baseloop_(loop),acceptor_(baseloop_,port),threadNum_(config.server().ioThreads),started_(false),config_(config){
    iothreadPool_ = std::make_unique<EventLoopThreadPool>(baseloop_);
    acceptor_.setNewConnectionCallback([this](int fd){
        newConnection(fd);
    });
    threadPool_ = std::make_unique<infra::thread::ThreadPool>(config_.server().backgroundThreads,config_.server().backgroundQueueCapacity);
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
    auto maintenanceRepos=repos;//复制一份给维护任务
    if(repos.hasSqlPool()){
        healthService_->setSqlPool(repos.sqlPool);
    }
    imService_->setRepositories(std::move(repos));
    imService_->loadFromRepositories();
    //注入在线连接数provider
    healthService_->setOnlineConnectionProvider([this](){
        return connections_.size();
    });

    if(config_.maintenance().enabled){
        maintenanceService_=std::make_unique<infra::maintenance::MaintenanceService>(config_.maintenance(),maintenanceRepos);
        healthService_->setMaintenanceProvider([this](){
            if(!maintenanceService_){
                return infra::maintenance::MaintenanceSnapshot{};
            }
            return maintenanceService_->snapshot();
        },config_.maintenance().intervalMs);
    }
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
    if(!stopping_.load(std::memory_order_acquire)){
        LOG_WARN("TcpServer destroyed without explicit stop");
    }
}

void TcpServer::start(){
    if(started_)
        return;
    started_=true;
    iothreadPool_->setThreadNum(threadNum_);
    iothreadPool_->start();
    acceptor_.listen();
    if(config_.health().enabled()){
        healthTimerId_=baseloop_->runEvery(std::chrono::milliseconds(config_.health().logIntervalMs()),[this](){
            if(stopping_.load(std::memory_order_acquire)){
                return ;
            }
            auto snapshot=healthService_->snapshot();
            LOG_INFO(infra::health::formatHealthSnapshot(snapshot));
        });
    }
    //接入定时任务
    if(maintenanceService_&&config_.maintenance().enabled){
        maintenanceTimerId_=baseloop_->runEvery(std::chrono::milliseconds(config_.maintenance().intervalMs),[this]{
            if(stopping_.load(std::memory_order_acquire)){
                return ;
            }
            auto result=threadPool_->submit([this]{
                auto stats=maintenanceService_->runOnce();
                if(!stats.ok){
                    LOG_WARN("maintenance failed: " + stats.error);
                }
                 else if (stats.totalDeleted() > 0) {
                    LOG_INFO("maintenance deleted rows=" + std::to_string(stats.totalDeleted()));
                }
            });
            if(result==infra::thread::TaskSubmitResult::QueueFull){
                LOG_WARN("TaskQueue full");
            }
        });
    }
}
//从Acceptor接收到新连接，在EventLoopThreadPool中选择一个IO线程，创建TcpConnection对象，
//再投递到baseloop保存到connections_中
void TcpServer::newConnection(int fd){
    if(stopping_.load(std::memory_order_relaxed)){
        ::close(fd);
        return ;
    }
    //检测fd重复
    if(connections_.find(fd)!=connections_.end()){
        LOG_ERROR("Duplicate fd "+std::to_string(fd)+" received in newConnection, closing it");
        ::close(fd);
        return;
    }
    EventLoop* ioloop=iothreadPool_->getNextLoop();
    ioloop->runInLoop([this,fd,ioloop](){
        if(stopping_.load(std::memory_order_acquire)){
            ::close(fd);
            return ;
        }
        auto conn=std::make_shared<TcpConnection>(ioloop,fd,this,config_);
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
            if(stopping_.load(std::memory_order_acquire)){
                conn->forceClose();
                return;
            }
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
    if(imService_){
        imService_->onDisconnect(conn);//通知IM业务连接断开，清理状态
    }
    tryFinishStopInBaseLoop();//连接被移除后检查是否正在关闭
}

void TcpServer::onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& msg){
    //转发给IM业务对象处理
    if(stopping_.load(std::memory_order_acquire)){
        return;
    }
    imService_->onMessage(conn,msg);    
}
void TcpServer::setThreadNum(int numThreads){
    threadNum_=numThreads;
}

void TcpServer::addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    connections_[fd]=std::move(conn);
}

void TcpServer::stop(){
    if(baseloop_->isInLoopThread()){
        stopInBaseLoop();
    }
    else{
        baseloop_->runInLoop([this](){
            stopInBaseLoop();
        });
    }
}

void TcpServer::stopInBaseLoop(){
    if(stopping_.load(std::memory_order_relaxed)){
        return ;
    }
    stopping_.store(true,std::memory_order_release);
    if(healthTimerId_.valid()){
        baseloop_->cancel(healthTimerId_);
        healthTimerId_=TimerId{};
    }
    if(maintenanceTimerId_.valid()){
        baseloop_->cancel(maintenanceTimerId_);
        maintenanceTimerId_=TimerId{};
    }
    if(maintenanceService_){
        maintenanceService_->requestStop();
    }
    //日志打印
    LOG_WARN("TcpServer stopping...");
    acceptor_.stop();//停止接收新连接
    closeAllConnections();//关闭已有连接
    
    tryFinishStopInBaseLoop();//若当前没有连接，直接完成关闭，若还有连接，等后续再触发
}

void TcpServer::closeAllConnections(){
    std::vector<std::shared_ptr<TcpConnection>> conns;
    //复制连接防止迭代器失效
    for(const auto& conn:connections_){
        conns.emplace_back(conn.second);
    }
    LOG_WARN("Closing all connections, count=" + std::to_string(conns.size()));
    for(auto& conn:conns){
        conn->forceClose();
    }
}

void TcpServer::setQuitCallback(std::function<void()>cb){
    quitCallback_=std::move(cb);
}

void TcpServer::tryFinishStopInBaseLoop(){
    if(!stopping_.load(std::memory_order_acquire)){
        return ;
    }
    if(!connections_.empty()){
        return;
    }

    finishStopInBaseLoop();
}

void TcpServer::finishStopInBaseLoop(){
    if(stopped_){//已经结束，防止重复释放
        return ;
    }
    stopped_=true;
    LOG_WARN("TcpServer finishing stop...");
    threadPool_->stop(infra::thread::ThreadPoolStopMode::Drain);

    if(imService_){//释放业务层资源
        imService_->shutdown();
    }

#ifdef PROJECT_CHAT_ENABLE_REDIS
    if(redisClient_){//关闭Redis
        redisClient_->close();
    }
#endif

    if(iothreadPool_){//连接全部关闭后执行
        iothreadPool_->stop();
    }

    if(quitCallback_){
        quitCallback_();
    }

}