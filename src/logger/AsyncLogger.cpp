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
    {   
        std::lock_guard lk(mutex_);
        if(queue_.size()>=maxQueueSize_){
            droppedCount_++;
            return;
        }
        queue_.push_back(std::move(line));
    }
    cv_.notify_one();
}
void AsyncLogger::run(){
    while(true){
        {
            std::unique_lock lk(mutex_);
            cv_.wait_for(lk,flushInterval_,[this]{return !queue_.empty()||!running_;});
            if(queue_.empty()){
                continue;
            }
            buffer_.swap(queue_);
        }
        for(auto& line:buffer_){
            sink_->write(line);
        }
        sink_->flush();
        writtenCount_+=buffer_.size();
        lastFlushMs_=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        buffer_.clear();
    }
}
uint64_t AsyncLogger::droppedCount()const{
    return droppedCount_.load();
}
uint64_t AsyncLogger::writtenCount()const{
    return writtenCount_.load();
}
bool AsyncLogger::isRunning()const{
    return running_.load();
}