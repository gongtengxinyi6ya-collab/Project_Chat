#pragma once
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "LogSink.h"
//继承LogSink,实现write方法，将日志输出到文件
class FileSink:public LogSink{
public:
    FileSink(std::string filepath,bool jsonFormat);//
    void write(std::string_view line) override;
    ~FileSink();
private:
    int fd_{-1};//文件描述符
    std::string path_;
    bool jsonFormat_{false};
};

FileSink::FileSink(std::string filepath,bool jsonFormat):path_(std::move(filepath)),jsonFormat_(jsonFormat){
    fd_ = open(path_.c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd_==-1){
        throw std::runtime_error("Failed to open log file: "+path_);
    }
}
void FileSink::write(std::string_view line){
    if(fd_==-1){
        std::cerr<<"FileSink not initialized properly"<<std::endl;
        return;
    }
    //循环写入直到写完或者错误
    size_t totalWritten=0;
    while(totalWritten<line.size()){
        ssize_t written = ::write(fd_, line.data() + totalWritten, line.size() - totalWritten);
        if(written==-1){
            if(errno==EINTR){
                continue;//被信号中断，重试 
            }
            std::cerr<<"Failed to write to log file: "<<path_<<std::endl;
            break;
        }
        totalWritten += written;
    }

}
FileSink::~FileSink(){
    if(fd_!=-1){
        ::close(fd_);
    }
}