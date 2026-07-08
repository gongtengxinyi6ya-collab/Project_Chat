#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread:: EventLoopThread(const std::string& name):loop_(nullptr),exiting_(false),name_(name)
{

}
EventLoopThread:: ~EventLoopThread(){
    stop();
}

void EventLoopThread::stop(){
    exiting_=true;
    EventLoop* loop=nullptr;
    {
        std::lock_guard lk(mutex_);
        loop=loop_;
    }
    if(loop!=nullptr){
        loop->quit();
    }
    if(thread_.joinable()){
        thread_.join();
    }
}
EventLoop* EventLoopThread:: startLoop(){
    thread_=std::thread(&EventLoopThread::threadFunc,this);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock,[this]{return loop_!=nullptr;});
    }
    return loop_;
}
void EventLoopThread:: threadFunc(){
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_=&loop;
        cond_.notify_one();
    }
    loop.loop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}