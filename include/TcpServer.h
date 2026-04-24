#pragma once
#include "Acceptor.h"
#include <ThreadPool.h>
#include <memory>
#include <unordered_map>
#include "im/ImService.h"
#include "logger/LogMacros.h"
#include "config/AppConfig.h"


class EventLoop;
class EventLoopThreadPool;
class TcpConnection;

//管理所有客户端连接，创建TcpConnection,删除/关闭连接，处理聊天逻辑
//在主线程中监听新连接，分发到IO线程处理，IO线程中创建TcpConnection对象，保存到connections_中
class TcpServer{
public:
    TcpServer(EventLoop* loop,int port,const AppConfig& config);
    ~TcpServer();

    void start();//启动服务器
    void newConnection(int clientfd,const AppConfig& config);//在baseLoop线程中处理新连接，创建TcpConnection对象，并保存到connections_中
    void removeConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn);//在baseLoop线程中删除连接，供TcpConnection调用
    void onMessage(const std::shared_ptr<TcpConnection>& conn, const std::string& msg);//处理消息，转发给其他客户端

    void setThreadNum(int numThreads);
    void addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn);//在主线程中添加连接，供TcpConnection调用
    
private:
    EventLoop* baseloop_;
    std::unique_ptr<EventLoopThreadPool> iothreadPool_;//线程池，分发IO线程
    int threadNum_;//IO线程数量
    bool started_;//服务器是否已经启动

    Acceptor acceptor_;//监听客户端连接
    // 使用 unique_ptr 让连接自动释放，避免手动 delete
    std::unordered_map<int,std::shared_ptr<TcpConnection>> connections_;//管理所有连接，key为fd，value为TcpConnection对象指针
    std::unique_ptr<ThreadPool> threadPool_;//线程池，处理消息转发等耗时操作

    //IM系统
    std::unique_ptr<im::Imservice> imService_;//IM业务对象，处理消息逻辑
    
};