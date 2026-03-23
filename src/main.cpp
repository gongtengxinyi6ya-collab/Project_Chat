#include "TcpServer.h"

#include <iostream>
int main()
{
    EventLoop loop;
    TcpServer server(&loop,8080);
    server.setThreadNum(4);
    server.start();
    loop.loop();
    return 0;
}
    