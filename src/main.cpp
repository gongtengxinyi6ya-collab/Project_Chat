#include "TcpServer.h"
#include <iostream>
int main()
{
    
    EventLoop loop;
    TcpServer server(&loop,8080);
    server.start();
    
}