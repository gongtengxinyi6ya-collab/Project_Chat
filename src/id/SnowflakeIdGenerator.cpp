#include "id/SnowflakeGenerator.h"
#include <stdexcept>
#include <thread>
#include <chrono>
snowflakeId::SnowflakeIdGenerator::SnowflakeIdGenerator(uint16_t nodeId, uint64_t epochMs)
:nodeId_(nodeId),epochMs_(epochMs){
    auto nowMs=currentMs();
    if(nodeId_>1023||epochMs_>=nowMs){
        throw std::invalid_argument("nodeId or epochMs exceed");
    }
}


uint64_t snowflakeId::SnowflakeIdGenerator::currentMs()const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
uint64_t snowflakeId::SnowflakeIdGenerator::nextId(){
    //加锁保证线程安全
    std::lock_guard lk(mutex_);
    //获取系统毫秒时间
    uint64_t nowMs=currentMs();
    if(nowMs<lastTimestampMs_){//时钟回拨
        auto diff=lastTimestampMs_-nowMs;
        if(diff<=5){//小幅回拨等待即可
            while(nowMs<=lastTimestampMs_){
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                nowMs=currentMs();
            }
        }
        else{
            //大幅回拨报异常
            throw std::runtime_error("system clock moved backwards too much");
        }
    }
    if(nowMs==lastTimestampMs_){//同一毫秒递增序列
        sequence_=(sequence_+1)&4095;
        if(sequence_==0){//序列达到4095时等待下一毫秒
            while(nowMs<=lastTimestampMs_){
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                nowMs=currentMs();
            }
        }
    }
    else{//新毫秒序列重置为0
        sequence_=0;
    }
    lastTimestampMs_=nowMs;
    if (nowMs < epochMs_) {
        throw std::runtime_error("current time is before epoch");
    }
    uint64_t timestampPart=(nowMs-epochMs_)<<kTimestampShirft_;
    uint64_t nodePart=nodeId_<<kNodeIdShirft_;
    return timestampPart|nodePart|sequence_;
}
std::string snowflakeId::SnowflakeIdGenerator::nextStringId(const std::string& prefix){
    return prefix+std::to_string(nextId());
}