#include "logger/AsyncLogger.h"

AsyncLogger::AsyncLogger(std::unique_ptr<LogSink> sink,size_t maxQueueSize,std::chrono::milliseconds flushInterval)
:sink_(std::move(sink)),maxQueueSize_(maxQueueSize),flushInterval_(flushInterval)
{
}
AsyncLogger::~AsyncLogger(){
    stop();
}
void AsyncLogger::start(){
    if(running_){
        return;
    }
    worker_=std::thread(&AsyncLogger::run,this);
    running_=true;
}
void AsyncLogger::stop(){
    if(!running_){
        return;
    }
    running_=false;
    cv_.notify_all();
    if(worker_.joinable()){
        worker_.join();
    }
}
void AsyncLogger::append(std::string line){
    std::lock_guard lk(mutex_);
    {
        if(queue_.size()>=maxQueueSize_){
            droppedCount_++;
            return;
        }
        queue_.push_back(std::move(line));
    }
    cv_.notify_one();
}
void AsyncLogger::run(){
    while(running_&&!queue_.empty()){
        
    }
}