#include <cerrno>
#include <cstring>
#include <string>
#include "Logger.h"


/*宏接口：基础宏实现分级和自动源信息
errno友好宏
*/
//基础宏
#define LOG_DEBUG(msg) \
    do{\
        Logger::instance().log(LogLevel::DEBUG,msg,__FILE__,__LINE__,__func__);\
    } while(0)
#define LOG_INFO(msg) \
    do{\
        Logger::instance().log(LogLevel::INFO,msg,__FILE__,__LINE__,__func__);\
    } while(0)
#define LOG_WARN(msg) \
    do{\
        Logger::instance().log(LogLevel::WARN,msg,__FILE__,__LINE__,__func__);\
    } while(0)
#define LOG_ERROR(msg) \
    do{\
        Logger::instance().log(LogLevel::ERROR,msg,__FILE__,__LINE__,__func__);\
        } while(0)
#define LOG_FATAL(msg) \
    do{Logger::instance().log(LogLevel::FATAL,msg,__FILE__,__LINE__,__func__);} while(0)

//errno友好宏
#define LOG_SYSERR(msg) \
    do{\
        int e=errno;\
        Logger::instance().log(LogLevel::ERROR,std::string(msg)+" errno= "+std::to_string(e)+" ("+std::string(std::strerror(e))+")",__FILE__,__LINE__,__func__);\
    } while(0)
#define LOG_SYSFATAL(msg) \
    do{\
        int e=errno;\
        Logger::instance().log(LogLevel::FATAL,std::string(msg)+" errno= "+std::to_string(e)+" ("+std::string(std::strerror(e))+")",__FILE__,__LINE__,__func__);\
    } while(0) 
    