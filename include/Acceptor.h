#pragma once

#include "Socket.h"
#include <memory>
#include <string>
#include <cstdint>
#include <functional>
//监听客户端连接,accept连接，触发新连接回调

class EventLoop;
class Channel;
class Acceptor{
    using NewConnectionCallback=std::function<void(int)>;
public:
    Acceptor(EventLoop* loop,std::string host,uint16_t port ,int backlog ,bool tcpNoDelay,bool keepAlive);
    ~Acceptor();
    void setNewConnectionCallback(NewConnectionCallback cb);
    void listen();
    void stop();//停止监听新连接
    bool listening()const{return listening_;}
private:
    EventLoop* loop_{nullptr};
    Socket listenSocket_;

    std::string host_;//监听地址
    uint16_t port_{0};//监听端口
    int backlog_{0};//连接等待队列长度

    bool tcpNoDelay_{true};//连接是否开启TCP_NODELAY
    bool keepAlive_{true};//是否开启内核KeepAlive

    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;

    bool listening_{false};//仅允许baseLoop线程访问
    int idleFd_{-1};//处理EMFILE

    void handleRead();//循环accept
    void handleFdExhaustion() noexcept;//进程fd耗尽也可从监听队列移除一个连接
    bool configureClientSocket(int fd) noexcept;
};