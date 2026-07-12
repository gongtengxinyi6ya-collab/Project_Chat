#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <cstdint>
#include "EventLoop.h"
#include "Channel.h"
#include "Buffer.h"

#include "config/AppConfig.h"
//向前声明
class ThreadPool;
class TcpServer;


const int BUFFERSIZE=4096;
//管理一个客户端连接对象，处理读写数据，关闭连接
//所有TcpConnection对象都保存在IO线程中，保证线程安全，非IO线程通过runInLoop将任务转发到IO线程执行
class TcpConnection
:public std::enable_shared_from_this<TcpConnection>{
    using CloseCallback=std::function<void(const std::shared_ptr<TcpConnection>&)>;//连接关闭回调，参数为当前连接对象的shared_ptr
    using MessageCallback=std::function<void(const std::shared_ptr<TcpConnection>&,const std::string&)>;//消息回调，参数为fd和消息内容

    using HighWaterCallback=std::function<void(const std::shared_ptr<TcpConnection>&,size_t)>;//高水位回调
    using LowWaterCallback=std::function<void(const std::shared_ptr<TcpConnection>&,size_t)>;//低水位回调
public:
    TcpConnection(EventLoop* loop,int fd,TcpServer* server,const AppConfig& config);
    ~TcpConnection();
    void handleRead();//读取客户端数据；
    void handleWrite();//发送outputBuffer数据

    void send(const std::string &msg);//线程安全入口，非io线程调用，转发到io线程执行发送
    void sendInLoop(const std::string& msg);//在IO线程中发送数据，真正执行发送逻辑，发送失败则保存到outputBuffer_中，并注册写事件，等待下一次发送机会
    void handleClose();//处理客户端关闭
    void handleError();//socket错误处理
    void handleError(int socketError);
    void setCloseCallback(CloseCallback cb);
    void setMessageCallback(MessageCallback cb);
    void setHighWaterCallback(HighWaterCallback cb);//上层注册高水位事件
    void setLowWaterCallback(LowWaterCallback cb);//上层注册恢复事件
    
    void forceClose();//对外线程安全接口

    EventLoop* getLoop() const;//返回所属的EventLoop，供服务器转发消息时调用
    int fd() const;
    //真实ip获取
    const std::string& peerIp()const{return peerIp_;};
    uint16_t peerPort()const{return peerPort_;};
    
    void connectionEstablished();//连接建立，注册事件
    void connectionDestroyed();//连接销毁，取消事件,最终释放fd
    void closeFd();
    bool isClosed() const{return !connected_.load();}//连接是否已关闭
    bool canSend(size_t payloadBytes)const;//可发送判断
    //心跳检测接口
    
    void startHeartbeat();//在建立连接后启动心跳周期任务
    void stopHeartbeat();//连接关闭时取消心跳定时器
    bool handleControlFrame(const std::string& payload);//拦截处理控制帧，不进入广播


    //背压接口
    size_t pendingBytes() const;//返回outputBuffer_可读字节数
    bool isOverloaded()const;//返回是否过载
    bool canAccept(size_t nextBytes)const;//返回是否能接受下一条消息，判断条件是pendingBytes()+nextBytes<=hardLimit
    uint64_t droppedMessage()const;//返回已丢弃消息数
    uint32_t overloadDropCount()const;//返回脸书过载丢弃次数
    void recordDrop(size_t payloadByres);//
private:
    EventLoop* loop_;//
    int fd_;//客户端socket

    TcpServer* server_;//服务器对象指针，调用服务器的消息转发函数
    std::unique_ptr<Channel> channel_;//事件监听
    bool fdClosed_{false};//fd是否已关闭，防止重复关闭和发送数据
    Buffer outputBuffer_;//待发送数据
    Buffer inputBuffer_;//读入缓存
    
    std::atomic<bool> connected_;
    CloseCallback closeCallback_;//删除连接回调
    MessageCallback messageCallback_;//消息回调，保存服务器注册的消息回调函数

    //注册地址ip
    std::string peerIp_{};//保存peer地址
    uint16_t peerPort_{0};//端口

    //心跳检测
    TimerId heartbeatTimerId_;//周期定时器句柄
    std::chrono::steady_clock::time_point lastPong_;//最近收到Pong的时间
    std::chrono::steady_clock::time_point lastActiveTime_;//最近收到业务帧
    std::chrono::steady_clock::time_point lastHeartbeeatTime_;//最近收到心跳帧
    std::chrono::milliseconds heartbeatInterval_;
    std::chrono::milliseconds heartbeatTimeout_;
    void onHeartbeatTick();//每次心跳定时器触发时执行，检测超时并发送ping

    size_t maxFrameLen;//最大消息长度，超过认为协议错误，关闭连接

    //广播背压
    HighWaterCallback highWaterCallback_;//高水位回调
    LowWaterCallback lowWaterCallback_;
    size_t highWaterMark_;//高水位限制,超过该值认为过载，触发限频措施，如丢弃消息、降低服务质量等
    size_t lowWaterMark_;//低水位限制,从过载恢复到正常的阈值
    size_t hardLimit_;//硬限制，超过直接丢弃消息不予接受

    std::atomic<bool> overloaded_{false};//是否过载
    std::atomic<uint64_t> droppedMessage_{0};//已丢弃消息计数，用于日志记录和监控
    std::atomic<uint32_t> overloadDropCount_{0};//过载丢弃次数计数，用于监控过载事件频率
    uint32_t maxOverloadDropCount_{0};//过载丢弃次数上限，超过该次数可以考虑关闭连接或触发更严重的限流措施
    std::atomic<size_t> pendingBytesEstimate_{0};//跨线程可读的待发送字节估算值，代替baseloop读取outputBuffer_
    void scheduleCloseInLoop();//保证关闭逻辑一定在连接所属ioloop执行
    void updatePendingEstimate();//同步获取outputBuffer可读字节

    void forceCloseInLoop();
};