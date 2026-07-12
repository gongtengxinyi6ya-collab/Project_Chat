#pragma once
#include <cstdint>
#include <string>

class Socket{
public:
    Socket();
    ~Socket();
    void bind(const std::string& host,uint16_t port);
    void listen(int backlog);

    int accept(int* savedErrno) noexcept;

    static bool setTcpNoDelay(int fd, bool enabled) noexcept;//禁用Nagle算法，减少小消息延迟
    static bool setKeepAlive(int fd, bool enabled) noexcept;//内核连接探测

    int fd()const noexcept;//返回fd

private:
    int listenfd_{-1};//保存监听socket
};



