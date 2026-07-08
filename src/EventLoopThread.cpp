#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread:: EventLoopThread(const std::string& name):loop_(nullptr),exiting_(false),name_(name)
{

}
EventLoopThread:: ~EventLoopThread(){
    exiting_=true;
    if(loop_!=nullptr){
        loop_->wakeup();
        loop_->quit();
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
}