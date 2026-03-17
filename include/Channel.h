#pragma once
#include <functional>
#include <sys/epoll.h>
class EventLoop;

//Channel 负责管理fd,事件类型读写/关闭/错误以及回调

class Channel{
    
public:
    using EventCallback=std::function<void()>;
    Channel(EventLoop* loop,int fd);
    int fd() const;
    void setReadCallback(EventCallback cb);//读回调
    void setWriteCallback(EventCallback cb);//写回调
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);
    void handleEvent();

    void setRevents(int revent);//设置实际事件
    void enableReading();//开启读事件
    void enableWriting();//开启写事件
    void disableWritng();//关闭写事件
    void disableAll();//关闭所有事件
    int events() const;
    bool inEpoll() const;
    void setInEpoll(bool inEpoll);
private:
    int fd_;//关联文件描述符
    int events_;//关注的事件
    int revents_;//实际发生的事件
    bool inEpoll_;//是否在epoll中,判断epoll_ctl是添加还是修改事件
    EventLoop* loop_;//
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;


};