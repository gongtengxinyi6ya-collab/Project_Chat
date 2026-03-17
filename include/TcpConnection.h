#include <string>
#include "EventLoop.h"
#include "Channel.h"

const int BUFFERSIZE=4096;
//管理一个客户端连接对象，处理读写数据，关闭连接
class TcpConnection{
    using CloseCallback=std::function<void(int)>;
    using MessageCallback=std::function<void(int ,const std::string&)>;//消息回调，参数为fd和消息内容
public:
    TcpConnection(EventLoop* loop,int fd);
    ~TcpConnection();
    void handleRead();//读取客户端数据；

    void handleWrite();//发送outputBuffer数据
    void send(const std::string &msg);//向客户端发送数据
    void handleClose();//处理客户端关闭
    void handleError();//socket错误处理
    void setCloseCallback(CloseCallback cb);
    void setMessageCallback(MessageCallback cb);
private:
    EventLoop* loop_;//
    int fd_;//客户端socket
    Channel* channel_;//事件监听
    std::string inputBuffer_;//接受客户端数据
    std::string outputBuffer_;//待发送数据
    bool connection_;
    CloseCallback closeCallback_;//删除连接回调
    MessageCallback messageCallback_;//消息回调，保存服务器注册的消息回调函数
};