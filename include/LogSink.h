#include <string_view>
#include <memory>
//日志输出抽象接口
class LogSink{
public:
    virtual ~LogSink()=default;
    virtual void write(std::string_view line)=0;
    virtual void flush(){}
};

