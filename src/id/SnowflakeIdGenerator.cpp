#include "id/SnowflakeGenerator.h"
#include <stdexcept>
snowflakeId::SnowflakeIdGenerator::SnowflakeIdGenerator(uint16_t nodeId, uint64_t epochMs)
:nodeId_(nodeId),epochMs_(epochMs){
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(nodeId_<=1023||epochMs_>=nowMs){
        throw std::invalid_argument("nodeId or epochMs exceed");
    }
}
uint64_t snowflakeId::SnowflakeIdGenerator::nextId(){
    //加锁保证线程安全
    std::lock_guard lk(mutex_);
    //获取系统毫秒时间
    auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(nowMs<lastTimestampMs_){//时钟回拨
        auto diff=lastTimestampMs_-nowMs;
        if(diff<=5){//小幅回拨
            nowMs=
        }
        else{
            //大幅回拨报异常
            throw std::runtime_error("system clock moved backwards too much");
        }
    }
    if(nowMs==lastTimestampMs_){//同一毫秒递增序列
        sequence_=sequence_+1;

    }
}