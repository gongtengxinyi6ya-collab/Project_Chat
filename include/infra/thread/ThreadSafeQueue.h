#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include <cstdint>
#include "infra/thread/TheadType.h"
/*线程安全队列，负责保护内部容器，非阻塞入队，阻塞出队
有界容量，关闭后拒绝新元素
唤醒正在等待的消费者
Drain/Discard队列内容*/

namespace infra::thread{
template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t capacity=0);

    ThreadSafeQueue (const ThreadSafeQueue&)=delete;//禁止复制构造
    ThreadSafeQueue& operator=(const ThreadSafeQueue& )=delete;//禁止赋值构造
    ThreadSafeQueue(ThreadSafeQueue&&)=delete;//禁止移动构造
    ThreadSafeQueue& operator=(ThreadSafeQueue&&)=delete;//禁止移动赋值

    QueuePushResult push(T value);
    bool tryPop(T& value);//尝试取出一个元素，如果队列为空返回false
    bool waitPop(T& value);//等待并取出一个元素，如果队列为空则阻塞
    void close(QueueCloseMode mode=QueueCloseMode::Drain);

    //查询接口
    bool closed()const;
    bool empty() const;
    size_t size()const;
    size_t capacity()const noexcept;
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condVar_;

    size_t capacity_{0};//当为0时表示无界队列
    bool closed_{false};//
};




template <typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(size_t capacity)
:capacity_(capacity){

}

template <typename T>
QueuePushResult ThreadSafeQueue<T>::push(T value){
    {
        std::lock_guard lk(mutex_);
        if(closed_){
            return QueuePushResult::Closed;
        }
        if(capacity_!=0&&queue_.size()>=capacity_){
            //当有容量限制且队列已满
            return QueuePushResult::Full;
        }
        queue_.push(std::move(value));
    }

    condVar_.notify_one();
    return QueuePushResult::Ok;
}

template <typename T>
bool ThreadSafeQueue<T>::waitPop(T& value){
    std::unique_lock lk(mutex_);
    condVar_.wait(lk,[this]{return closed_||!queue_.empty();});
    if(queue_.empty()){//被唤醒后，队列为空则返回
        return false;
    }
    value = std::move(queue_.front());
    queue_.pop();
    return true;
}

template <typename T>
bool ThreadSafeQueue<T>::tryPop(T& value){
    std::lock_guard lk(mutex_);
    if(queue_.empty()){
        return false;
    }
    value=std::move(queue_.front());
    queue_.pop();
    return true;
}

template <typename T>
void ThreadSafeQueue<T>::close(QueueCloseMode mode = QueueCloseMode::Drain){
    {
        std::lock_guard lk(mutex_);
        if(closed_){
            return;
        }
        closed_=true;
        if(mode==QueueCloseMode::Discard){
            while(!queue_.empty()){//Discard清空queue
                queue_.pop();
            }
        }
    }
    condVar_.notify_all();//通知所有线程退出阻塞

}

template <typename T>
bool ThreadSafeQueue<T>::empty() const{
    std::lock_guard lk(mutex_);
    return queue_.empty();
}

template <typename T>
size_t ThreadSafeQueue<T>::size() const{
    std::lock_guard lk(mutex_);
    return queue_.size();
}

template <typename T>
bool ThreadSafeQueue<T>::closed() const{
    std::lock_guard lk(mutex_);
    return closed_;
}

template <typename T>
size_t ThreadSafeQueue<T>::capacity() const noexcept{
    std::lock_guard lk(mutex_);
    return capacity_;
}


}