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
#include "infra/thread/KeyedSerialExecutor.h"
#include "net/OutboundFrame.h"
#ifdef PROJECT_CHAT_ENABLE_REDIS
#include "infra/redis/RedisClient.h"
#include "security/rate_limit/RedisRateLimitStore.h"
#include "security/rate_limit/RateLimiter.h"
#endif
#include <vector>
#include <unordered_map>
TcpServer::TcpServer(EventLoop* loop,int port,const AppConfig& config)
:baseloop_(loop),config_(config),acceptor_(baseloop_,config_.server().host,port,config_.server().backlog,config_.net().tcpNoDelay,config_.net().keepAlive),threadNum_(config.server().ioThreads),started_(false){
    iothreadPool_ = std::make_unique<EventLoopThreadPool>(baseloop_);
    acceptor_.setNewConnectionCallback([this](int fd){
        newConnection(fd);
    });
    threadPool_ = std::make_unique<infra::thread::ThreadPool>(config_.server().backgroundThreads,config_.server().backgroundQueueCapacity);
    if(config_.messageAsync().enabled){
        messageExecutor_=std::make_unique<infra::thread::KeyedSerialExecutor>(config_.messageAsync().workerThreads,config_.messageAsync().queueCapacity);
    }
    imService_ = std::make_unique<im::Imservice>(1,config_.im(),config_.id());
    imService_->setBatchSender([this](const std::vector<net::ConnKey>& keys,net::SharedPayload payload){
        baseloop_->assertInLoopThread();
        return sendBatchToConnKeys(keys,std::move(payload));
    });
    if(messageExecutor_){
        imService_->setMessageAsyncExecutor(
            [this](const std::string& groupId,std::function<void()>task)->infra::thread::TaskSubmitResult{
                //注入工作任务提交
                if(!messageExecutor_){
                    return infra::thread::TaskSubmitResult::Stopping;
                }
                return messageExecutor_->submit(groupId,std::move(task));
            },
            [this](std::function<void()>task)->bool{
                if(!baseloop_||!task){
                    return false;
                }
                baseloop_->queueInLoop(std::move(task));
                return true;
            }
        );
    }
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
    if(messageExecutor_){
        healthService_->setMessageExecutorStatsProvider([this](){
            if(!messageExecutor_){
                return infra::thread::ThreadPoolStats{};
            }
            return messageExecutor_->aggregateStats();
        },config_.messageAsync().queueWarnPercent);
    }
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
            std::weak_ptr<TcpConnection> weakConn=conn;
            baseloop_->runInLoop([this,weakConn,msg](){
                auto conn=weakConn.lock();
                if(!conn||conn->isClosed()||stopping_.load(std::memory_order_acquire)){
                    return;
                }
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
    if(!conn||conn->isClosed()||stopping_.load(std::memory_order_acquire))
    {
        return ;
    }
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
    if(imService_){
        imService_->stopAcceptingAsyncMessages();
    }
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
    if (finalStopQueued_) {
        return;
    }

    finalStopQueued_ = true;

    if (messageExecutor_) {
        messageExecutor_->stop(infra::thread::ThreadPoolStopMode::Drain);
    }

    baseloop_->queueInLoop([this]() {
        finalizeStopInBaseLoop();
    });

}

void TcpServer::finalizeStopInBaseLoop(){
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

//批量广播接口

net::BatchSendResult TcpServer::sendBatchToConnKeys(const std::vector<net::ConnKey>& keys, net::SharedPayload payload){
    //baseLoop执行
    net::BatchSendResult result;
    if(!payload){
        result.failed=keys.size();
        return result;
    }
    auto frame=net::OutboundFrame::create(std::move(payload),config_.net().maxFrameLen);//整批创建一次
    if(!frame){
        result.failed=keys.size();
        return result;
    }
    std::unordered_map<EventLoop*,std::vector<std::shared_ptr<TcpConnection>>> batches;
    //遍历连接
    for(const auto key:keys){
        auto it=connections_.find(static_cast<int>(key));
        if(it==connections_.end()){
            //找不到连接
            result.add(net::SendResult::NoSuchConnection);
            continue;
        }
        const auto& connection=it->second;
        const auto reserveResult=connection->tryReserveFrame(frame->frameBytes());

        result.add(reserveResult);
        if(reserveResult!=net::SendResult::Ok){
            continue;
        }

        batches[connection->getLoop()].push_back(connection);//根据loop加入对应batch

    }
    //遍历batches
    for(auto& batch:batches){
        enqueueFrameBatch(batch.first,std::move(batch.second),frame);
    }
    return result;
}

void TcpServer::enqueueFrameBatch(EventLoop* ioLoop, std::vector<std::shared_ptr<TcpConnection>> connections, std::shared_ptr<const net::OutboundFrame> frame){
    if(!ioLoop||!frame||connections.empty()){
        if(frame){//若之前预留成功则撤销预留
            for(const auto&conn:connections){
                if(conn){
                    conn->releaseReservedFrame(frame->frameBytes());
                }
            }
        }
        return;
    }
    ioLoop->queueInLoop([connections=std::move(connections),frame=std::move(frame)]()mutable{
        //遍历连接调用
        for(auto& conn:connections){
            if(!conn){
                continue;
            }
            conn->sendReservedFrameInLoop(frame);
        }
    });

}