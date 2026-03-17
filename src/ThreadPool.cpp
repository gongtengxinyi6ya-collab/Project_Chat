#include "ThreadPool.h"

JoinThreads::JoinThreads(std::vector<std::thread>& threads):threads_(threads){}

JoinThreads::~JoinThreads(){
    for(auto& thread:threads_){
        if(thread.joinable())
            thread.join();
    }
}


ThreadPool::ThreadPool():stop_(false),joiner_(threads_){
    unsigned const threadCount=std::thread::hardware_concurrency();
    try{
        for(unsigned i=0;i<threadCount;++i){
            threads_.emplace_back(&ThreadPool::workerThread,this);
        }
    }catch(...){
        stop_=true;
        throw;  
}
}

ThreadPool::~ThreadPool(){
    stop_=true;
}

template<typename FunctionType>
void ThreadPool::submit(FunctionType func){
    taskQueue_.push(Task(func));
}

void ThreadPool::workerThread(){
    while(!stop_){
        Task task;
        if(taskQueue_.tryPop(task)){//取出任务并执行
            task();
        }else{
            std::this_thread::yield();//没有任务，当前线程让出CPU时间片
        }
    }
}