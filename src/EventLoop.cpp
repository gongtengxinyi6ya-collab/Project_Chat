#include "EventLoop.h"
#include "until.h"


EventLoop:: EventLoop():looping(false),quit_(false),epollfd_(-1),activeEvents_(EPOLL_MAX_EVENTS)
{
    epollfd_=epoll_create(EPOLL_MAX_EVENTS);
    if(epollfd_==-1){
        throw std::runtime_error("epoll_create failed");    
    }

}
EventLoop:: ~EventLoop(){

}
void EventLoop:: loop(){
    if(looping)
        return;

    looping=true;

    while(!quit_){
        int event_cnt=epoll_wait(epollfd_,activeEvents_.data(),EPOLL_MAX_EVENTS,-1);
        if(event_cnt<0)
        {
            if(errno==EINTR)//信号中断
                continue;
            perror("epoll_wait error");
            break;
        }
        
        for(int i=0;i<event_cnt;i++){
            int fd=activeEvents_[i].data.fd;
            auto it=channels_.find(fd);

            if(it!=channels_.end())
            {
                it->second->setRevents(activeEvents_[i].events);

                    try {
                        it->second->handleEvent();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in handleEvent: " << e.what() << std::endl;
                        // 可以选择关闭连接或处理错误
                    }
                }
                    

    }
}
    
}
void EventLoop::quit()
{
    quit_=true;
}

void EventLoop::addChannel(Channel* channel)
{
    int fd=channel->fd();
    if(!channel->inEpoll()){//新添加的fd
        //新添加的fd
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=EPOLLIN|EPOLLET;//默认关注读事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&ev)==-1){
            throw std::runtime_error("epoll_ctl add failed");
        }
        channel->setInEpoll(true);
        channels_[fd]=channel;
    }
    else{
        //已存在的fd，修改事件
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=EPOLLIN|EPOLLET;//默认关注读事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&ev)==-1){
            throw std::runtime_error("epoll_ctl mod failed");
        }
    }

}

void EventLoop::removeChannel(int fd)
{
    auto it=channels_.find(fd);
    if(it!=channels_.end()){
        if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,nullptr)==-1){
            throw std::runtime_error("epoll_ctl del failed");
        }
        channels_.erase(it);
    }
}

void EventLoop::updateChannel(Channel* channel)
{
    int fd=channel->fd();
    if(channel->inEpoll()){//已存在的fd，修改事件
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=channel->events()|EPOLLET;//关注channel关注的事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&ev)==-1){
            throw std::runtime_error("epoll_ctl mod failed");
        }
    }
    else{
        //新添加的fd
        struct epoll_event ev;
        ev.data.fd=fd;
        ev.events=channel->events()|EPOLLET;//关注channel关注的事件，边缘触发
        if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&ev)==-1){
            throw std::runtime_error("epoll_ctl add failed");
        }
        channel->setInEpoll(true);
        channels_[fd]=channel;
    }
}