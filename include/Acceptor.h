#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "logger/LogMacros.h"
//监听客户端连接,accept连接，触发新连接回调
class Acceptor{
    using NewConnectionCallback=std::function<void(int)>;
public:
    Acceptor(EventLoop* loop,int port);
    void setNewConnectionCallback(NewConnectionCallback cb);
    void listen();
private:
    void handleRead();
    int port_;
    Socket listenSocket_;
    EventLoop* loop_;
    Channel* channel_;
    NewConnectionCallback newConnectionCallback_;
};