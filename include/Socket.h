#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <cstring>
#include <iostream>
#include "LogMacros.h"
#include "until.h"

class Socket{
public:
    Socket();
    ~Socket();
    void bind(int port);
    void listen();
    int fd();//返回fd
    int accept();

private:
    int listenfd_;
};



