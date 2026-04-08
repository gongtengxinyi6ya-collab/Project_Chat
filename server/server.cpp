#include "TcpServer.h"
#include "Logger.h"
#include "FileSink.h"
#include "StderrSink.h"
#include <iostream>
int main()
{
    Logger::instance().setLevel(LogLevel::DEBUG);
    Logger::instance().setSink(std::make_unique<StderrSink>());
    EventLoop loop;
    TcpServer server(&loop,8080);
    server.setThreadNum(4);
    server.start();
    LOG_INFO("Server started on port 8080");
    loop.loop();
    return 0;
}
    