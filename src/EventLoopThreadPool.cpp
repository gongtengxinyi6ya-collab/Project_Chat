#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThreadPool:: EventLoopThreadPool(EventLoop* baseLoop):baseLoop_(baseLoop),started_(false),numThreads_(0),next_(0)
{

}
EventLoopThreadPool:: ~EventLoopThreadPool(){}

void EventLoopThreadPool::start(){
    if(started_)
        return;
    started_=true;

    for(int i=0;i<numThreads_;i++){
        auto thread=std::make_unique<EventLoopThread>();
        threads_.push_back(std::move(thread));
        loops_.push_back(threads_.back()->startLoop());
    }
}
EventLoop* EventLoopThreadPool:: getNextLoop(){
    if(loops_.empty())
        return baseLoop_;
    EventLoop* loop=loops_[next_];
    next_=(next_+1)%loops_.size();
    return loop;
}
void EventLoopThreadPool:: setThreadNum(int numThreads){
    if(started_)
        return;
    numThreads_=numThreads;
}

void EventLoopThreadPool::stop(){
    if(!started_){
        return;
    }
    for(auto loop :loops_){
        loop->quit();
    }
    threads_.clear();
    loops_.clear();
    started_=false;
}