#pragma once
#include "Acceptor.h"
#include "infra/thread/ThreadPool.h"
#include <memory>
#include <atomic>
#include <unordered_map>
#include <functional>

#include "config/AppConfig.h"
#include "storage/RepositoryFactory.h"
#include "timer/TimerId.h"
class EventLoop;
class EventLoopThreadPool;
class TcpConnection;
namespace im{
    class Imservice;
}
namespace infra::health{
    class HealthService;
}
namespace infra::redis{
    class RedisClient;
}
namespace infra::maintenance {
    class MaintenanceService;
}
//管理所有客户端连接，创建TcpConnection,删除/关闭连接，处理聊天逻辑
//在主线程中监听新连接，分发到IO线程处理，IO线程中创建TcpConnection对象，保存到connections_中
class TcpServer{
public:
    TcpServer(EventLoop* loop,int port,const AppConfig& config);
    ~TcpServer();

    void start();//启动服务器
    void newConnection(int clientfd);//在baseLoop线程中处理新连接，创建TcpConnection对象，并保存到connections_中
    void removeConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn);//在baseLoop线程中删除连接，供TcpConnection调用
    void onMessage(const std::shared_ptr<TcpConnection>& conn, const std::string& msg);//处理消息，转发给其他客户端

    void setThreadNum(int numThreads);
    void addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn);//在主线程中添加连接，供TcpConnection调用
    
    void stop();//线程安全停止入口
    bool isStopping()const {return stopping_.load(std::memory_order_relaxed);}

    void setQuitCallback(std::function<void()> cb);
private:
    EventLoop* baseloop_;
    Acceptor acceptor_;//监听客户端连接
    std::unique_ptr<EventLoopThreadPool> iothreadPool_;//线程池，分发IO线程
    int threadNum_;//IO线程数量
    bool started_;//服务器是否已经启动
    std::atomic<bool> stopping_{false};//标识服务正在关闭
    // 使用 unique_ptr 让连接自动释放，避免手动 delete
    std::unordered_map<int,std::shared_ptr<TcpConnection>> connections_;//管理所有连接，key为fd，value为TcpConnection对象指针
    std::unique_ptr<ThreadPool> threadPool_;//线程池，处理消息转发等耗时操作

    //IM系统
    std::unique_ptr<im::Imservice> imService_;//IM业务对象，处理消息逻辑
    
    //配置
    AppConfig config_;//服务器配置，传递给TcpConnection使用

    std::unique_ptr<infra::health::HealthService> healthService_;//健康检查
    TimerId healthTimerId_;
    
    //RedisClient
    std::shared_ptr<infra::redis::RedisClient> redisClient_;

    //
    std::function<void()> quitCallback_;//关闭完成后通知外部退出baseLoop
    bool stopped_{false};//关闭流程已经完成
    void stopInBaseLoop();//内部真正执行服务停止逻辑
    void closeAllConnections();//关闭当前所有连接
    void tryFinishStopInBaseLoop();//判断是否可以进入最终释放阶段
    void finishStopInBaseLoop();//完成最终释放

    //后台清理服务
    std::unique_ptr<infra::maintenance::MaintenanceService> maintenanceService_;
    
    TimerId maintenanceTimerId_;
};