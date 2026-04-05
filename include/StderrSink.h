#include <unistd.h>
#include "LogSink.h"
//继承LogSink,实现write方法，将日志输出到标准错误流
class StderrSink:public LogSink{
public:
    void write(std::string_view line) override{
        ::write(STDERR_FILENO,line.data(),line.size());
    }
};