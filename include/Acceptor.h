#pragma once

#include "Socket.h"
#include <memory>
#include <functional>
//监听客户端连接,accept连接，触发新连接回调

class EventLoop;
class Channel;
class Acceptor{
    using NewConnectionCallback=std::function<void(int)>;
public:
    Acceptor(EventLoop* loop,int port);
    void setNewConnectionCallback(NewConnectionCallback cb);
    void listen();
    void stop();//停止监听新连接
    bool listening()const{return listening_;}
private:
    void handleRead();
    int port_;
    Socket listenSocket_;
    EventLoop* loop_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_{false};//标识当前是否已经开始监听
};