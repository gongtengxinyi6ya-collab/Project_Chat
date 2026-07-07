#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include "LogSink.h"

/*
把Logger从每条日志同步写道Sink改为前台入队，后台批量落盘*/
class AsyncLogger{
public:
    AsyncLogger(std::unique_ptr<LogSink> sink,size_t maxQueueSize,std::chrono::milliseconds flushInterval);//注入Sink和参数
    ~AsyncLogger();
    void start();//启动后台线程
    void stop();//停止后台线程并刷尽队列
    void append(std::string line);//前台线程入队
    void run();//后台批量消费并输出
    uint64_t droppedCount()const;//返回累计丢弃日志数
    uint64_t writtenCount()const;//返回累计写入条数
    bool isRunning()const;
    size_t queueSize() const;//队列大小
private:
    std::mutex mutex_;//保护队列与状态
    std::condition_variable cv_;//唤醒后台线程
    std::vector<std::string> queue_;//前台写入队列
    std::vector<std::string> buffer_;//后台批量交换缓存，减少锁持有时间
    std::thread worker_;//后台刷盘线程
    std::atomic<bool> running_{false};//线程运行标志
    std::unique_ptr<LogSink> sink_;//真正输出目标
    size_t maxQueueSize_{10000};//队列上限
    std::chrono::milliseconds flushInterval_{100};//周期刷盘间隔
    std::atomic<uint64_t> droppedCount_{0};//队列满丢弃计数
    std::atomic<uint64_t> writtenCount_{0};//已写条数
    std::atomic<uint64_t> lastFlushMs_{0};//最近一次flush时间


};
