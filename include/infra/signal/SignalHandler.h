#pragma once
#include <memory>
#include <functional>
#include <atomic>
#include <csignal>
/*负责将Linux信号转换为EventLoop可处理的事件*/
class EventLoop;
class Channel;

namespace infra::signal{
class SignalHandler{
public:
    using SignalCallback=std::function<void(int)>;
    explicit SignalHandler(EventLoop* loop);//构造创建eventFd和Channel
    ~SignalHandler();//释放资源

    void setSignalCallback(SignalCallback cb);//注入收到信号后的处理逻辑
    void start();//开始监听信号
private:
    EventLoop* loop_;//所属baseLoop
    int eventFd_;//接收信号通知
    std::unique_ptr<Channel> channel_;//监听eventFd_可读事件
    SignalCallback signalCallback_;//收到信号后由baseLoop执行的回调
    static std::atomic<int> receivedSignal_;//信号编号
    static int notifyFd_;//向eventFd写入，唤醒loop

    void handleRead();//EventLoop中处理signal通知
    static void handleSignal(int signo);//C风格信号hanlder
};
}