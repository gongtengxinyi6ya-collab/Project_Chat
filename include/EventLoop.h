#pragma once

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unordered_map>
#include <sys/epoll.h>

#include "Channel.h"

const int EPOLL_MAX_EVENTS=100;
//负责事件循环，调用select/epoll,分发事件给Channel
class EventLoop{
 public:
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void addChannel(Channel* channel);
    void updateChannel(Channel* channel);
    void removeChannel(int fd);
private:

    bool looping;
    bool quit_;
    int epollfd_;//epoll文件描述符,用于管理所有fd的事件
    std::vector<struct epoll_event> activeEvents_;//epoll事件列表

    std::unordered_map<int,Channel*> channels_;

};

