#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>

template <typename T>

class ThreadSafeQueue {
public:
    ThreadSafeQueue();
    ThreadSafeQueue(const ThreadSafeQueue& other);
    ThreadSafeQueue& operator=(const ThreadSafeQueue& other)=delete;//禁止赋值操作

    void push(const T value);
    bool tryPop(T& value);//尝试取出一个元素，如果队列为空返回false
    std::shared_ptr<T> tryPop();//尝试取出一个元素，如果队列为空返回空指针

    void waitAndPop(T& value);//等待并取出一个元素，如果队列为空则阻塞
    std::shared_ptr<T> waitAndPop();//等待并取出一个元素，如果队列为空则阻塞
    bool empty() const;
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condVar_;
};





template <typename T>
ThreadSafeQueue<T>::ThreadSafeQueue()
{

}
template <typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(const ThreadSafeQueue& other)
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    queue_ = other.queue_;
}

template <typename T>
void ThreadSafeQueue<T>::push(const T value){
    std::lock_guard lk(mutex_);
    queue_.push(std::move(value));
    condVar_.notify_one();

}

template <typename T>
bool ThreadSafeQueue<T>::tryPop(T& value){
    std::lock_guard lk(mutex_);
    if(queue_.empty())
        return false;
    
    value = std::move(queue_.front());
    queue_.pop();
    return true;
}

template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::tryPop(){
    std::lock_guard lk(mutex_);
    if(queue_.empty())
        return std::shared_ptr<T>();
    std::shared_ptr<T> res(std::make_shared<T>(std::move(queue_.front())));
    queue_.pop();
    return res;
}

template <typename T>
void ThreadSafeQueue<T>::waitAndPop(T& value){
    std::unique_lock lk(mutex_);
    condVar_.wait(lk,[this]{return !queue_.empty();});
    value = std::move(queue_.front());
    queue_.pop();
}

template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::waitAndPop(){
    std::unique_lock lk(mutex_);
    condVar_.wait(lk,[this]{return !queue_.empty();});
    std::shared_ptr<T> res(std::make_shared<T>(std::move(queue_.front())));
    queue_.pop();
    return res;
}

template <typename T>
bool ThreadSafeQueue<T>::empty() const{
    std::lock_guard lk(mutex_);
    return queue_.empty();
}


