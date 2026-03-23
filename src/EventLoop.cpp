#include "EventLoop.h"
#include "until.h"


EventLoop:: EventLoop():looping(false),quit_(false),epollfd_(-1),activeEvents_(EPOLL_MAX_EVENTS),wakeupFd_(-1),threadId_(std::this_thread::get_id())
{
    epollfd_=epoll_create(EPOLL_MAX_EVENTS);
    if(epollfd_==-1){
        throw std::runtime_error("epoll_create failed");    
    }
    wakeupFd_=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(wakeupFd_==-1){
        throw std::runtime_error("eventfd failed");
    }
    wakeupChannel_=std::make_unique<Channel>(this,wakeupFd_);
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleWakeup,this));
    wakeupChannel_->enableReading();
    this->addChannel(wakeupChannel_.get());

}
EventLoop:: ~EventLoop(){

}
void EventLoop:: loop(){
    if(looping)
        return;

    looping=true;

    while(!quit_){
        int event_cnt=epoll_wait(epollfd_,activeEvents_.data(),EPOLL_MAX_EVENTS,1000);
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
        doPendingFunctors();
    
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

bool EventLoop::isInLoopThread() const{
    return threadId_==std::this_thread::get_id();
}
void EventLoop::runInLoop(std::function<void()> func){
    if(isInLoopThread()){
        func();
    }
    else{
        queueInLoop(std::move(func));
        wakeup();
    }
}
void EventLoop::queueInLoop(std::function<void()> func){
    {
        std::lock_guard lk(mutex_);
        pendingFunctors_.push_back(std::move(func));
    }
    if(!isInLoopThread()){
        wakeup();
    }
}

void EventLoop::wakeup(){
    uint64_t one=1;
    ssize_t n=write(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        std::cerr<<"EventLoop::wakeup() writes "<<n<<" bytes instead of 8"<<std::endl;
    }
}

void EventLoop::handleWakeup(){
    uint64_t one=1;
    ssize_t n=read(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        std::cerr<<"EventLoop::handleWakeup() reads "<<n<<" bytes instead of 8"<<std::endl;
    }
}
void EventLoop::doPendingFunctors(){
    std::vector<std::function<void()>> functors;
    {
        std::lock_guard lk(mutex_);
        functors.swap(pendingFunctors_);
    }
    for(const auto& func:functors){
        func();
    }

}