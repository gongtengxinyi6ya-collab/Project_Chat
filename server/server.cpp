#include "TcpServer.h"
#include "Logger.h"
#include <iostream>
int main()
{
    Logger::instance().setLevel(LogLevel::DEBUG);

    EventLoop loop;
    TcpServer server(&loop,8080);
    server.setThreadNum(4);
    server.start();
    LOG_INFO("Server started on port 8080");
    loop.loop();
    return 0;
}
    