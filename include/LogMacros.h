#include <cerrno>
#include <cstring>
#include <string>
#include "Logger.h"
#include "LogLevel.h"

/*宏接口：基础宏实现分级和自动源信息
errno友好宏
*/
//基础宏
#define LOG_DEBUG(msg) Logger::instance().log(DEBUG,msg,_FILE_,_LINE_,_func_)
#define LOG_INFO(msg) Logger::instance().log(INFO,msg,_FILE_,_LINE_,_func_)
#define LOG_WARN(msg) Logger::instance().log(WARN,msg,_FILE_,_LINE_,_func_)
#define LOG_ERROR(msg) Logger::instance().log(ERROR,msg,_FILE_,_LINE_,_func_)
#define LOG_FATAL(msg) Logger::instance().log(FATAL,msg,_FILE_,_LINE_,_func_)

//errno友好宏
#define LOG_SYSERR(msg) Logger::instance().log(ERROR,std::string(msg)+" errno= "+std::to_string(errno)+" ("+std::string(std::strerror(errno))+")",_FILE_,_LINE_,_func_)
#define LOG_SYSFATAL(msg) Logger::instance().log(FATAL,std::string(msg)+" errno= "+std::to_string(errno)+" ("+std::string(std::strerror(errno))+")",_FILE_,_LINE_,_func_)