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