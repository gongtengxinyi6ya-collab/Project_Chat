#pragma once
#include <string>
#include <memory>

#include "EventLoop.h"
#include "Channel.h"
class ThreadPool;
class TcpServer;

const int BUFFERSIZE=4096;
//管理一个客户端连接对象，处理读写数据，关闭连接
class TcpConnection
:public std::enable_shared_from_this<TcpConnection>{
    using CloseCallback=std::function<void(const std::shared_ptr<TcpConnection>&)>;//连接关闭回调，参数为当前连接对象的shared_ptr
    using MessageCallback=std::function<void(const std::shared_ptr<TcpConnection>&,const std::string&)>;//消息回调，参数为fd和消息内容
public:
    TcpConnection(EventLoop* loop,int fd,ThreadPool* threadPool,TcpServer* server);
    ~TcpConnection();
    void handleRead();//读取客户端数据；
    void handleWrite();//发送outputBuffer数据
    void send(const std::string &msg);//线程安全入口，非io线程调用，转发到io线程执行发送
    void sendInLoop(const std::string& msg);//在IO线程中发送数据
    void handleClose();//处理客户端关闭
    void handleError();//socket错误处理
    void setCloseCallback(CloseCallback cb);
    void setMessageCallback(MessageCallback cb);

    EventLoop* getLoop() const;
    int fd() const;
    
    void connectionEstablished();//连接建立，注册事件
    void connectionDestroyed();//连接销毁，取消事件

private:
    EventLoop* loop_;//
    int fd_;//客户端socket
    ThreadPool* threadPool_;//线程池，处理消息转发等耗时操作
    TcpServer* server_;//服务器对象指针，调用服务器的消息转发函数
    Channel* channel_;//事件监听
    
    std::string outputBuffer_;//待发送数据
    bool connection_;
    CloseCallback closeCallback_;//删除连接回调
    MessageCallback messageCallback_;//消息回调，保存服务器注册的消息回调函数
};