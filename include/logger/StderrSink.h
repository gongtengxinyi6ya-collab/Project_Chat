#pragma once
#include <unistd.h>
#include <errno.h>
#include "LogSink.h"
//继承LogSink,实现write方法，将日志输出到标准错误流
class StderrSink:public LogSink{
public:
    void write(std::string_view line) override{
        const char* p=line.data();
        size_t left=line.size();//
        while(left>0){
            ssize_t n=::write(2,p,left);
            if(n>0){
                p+=n;
                left-=n;
            }
            if(n==-1){
                if(errno==EINTR){
                    continue;
                }else{
                    break;
                }
            }
        }
        
    }
};