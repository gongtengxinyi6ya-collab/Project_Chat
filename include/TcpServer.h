#pragma once
#include "Acceptor.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include <ThreadPool.h>
#include <memory>
#include <unordered_map>
//管理所有客户端连接，创建TcpConnection,删除/关闭连接，处理聊天逻辑

class TcpServer{
public:
    TcpServer(EventLoop* loop,int port);
    ~TcpServer();

    void start();//启动服务器
    void newConnection(int clientfd);//
    void removeConnection(int fd);
    void onMessage(int fd,const std::string& msg);//处理消息，转发给其他客户端
private:
    EventLoop* loop_;
    Acceptor acceptor_;//监听客户端连接
    // 使用 unique_ptr 让连接自动释放，避免手动 delete
    std::unordered_map<int,std::unique_ptr<TcpConnection>> connections_;//保存所有客户端连接
    std::unique_ptr<ThreadPool> threadPool_;//线程池，处理消息转发等耗时操作
};