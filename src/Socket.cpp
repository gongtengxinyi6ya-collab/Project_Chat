#include "Socket.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <stdexcept>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "logger/LogMacros.h"
Socket::Socket(){
    listenfd_=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(listenfd_<0)
        throw::std::runtime_error("socket() failed");
}

Socket::~Socket(){
    close(listenfd_);
}
//绑定端口
void Socket::bind(const std::string& host,uint16_t port){
    if(host.empty()){
        throw std::invalid_argument("host invaild");
    }
    if(port==0){
        throw std::invalid_argument("port invaild");
    }
    int opt=1;
    if(::setsockopt(listenfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
    {//允许服务器重启时重新绑定端口
        throw std::runtime_error("setsockopt filed");
    }
    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    if(host=="0.0.0.0"){
        addr.sin_addr.s_addr=htonl(INADDR_ANY);
    }
    else{
        if(inet_pton(AF_INET,host.c_str(),&addr.sin_addr)!=1){
            throw std::runtime_error("invalid IP address");
        }
    }
    addr.sin_port=htons(port);
    if(::bind(listenfd_,(sockaddr*)&addr,sizeof(addr))==-1){
        LOG_SYSERR("bind() failed");
        throw::std::runtime_error("bind() failed");
    }
}
//监听
void Socket::listen(int backlog)
{   
    if(backlog<=0){
        throw std::invalid_argument("backlog invaild");
    }
    if(::listen(listenfd_,backlog)<0){
        throw::std::runtime_error("listen() failed");
    }
}

//接收客户端连接
int Socket::accept(int* savedErrno)noexcept{
    sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    socklen_t len=sizeof(client_addr);
    int clientfd=::accept4(listenfd_,(sockaddr*)&client_addr,&len,SOCK_NONBLOCK|SOCK_CLOEXEC);
    if(clientfd<0){
        if(savedErrno){
            *savedErrno=errno;
        }
        return -1;
    }
    return clientfd;

}

//TCP选项方法
bool Socket::setTcpNoDelay(int fd, bool enabled) noexcept{
    const int value=enabled?1:0;
    return setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&value,sizeof(value))==0;
}
bool  Socket::setKeepAlive(int fd, bool enabled) noexcept{
    const int value=enabled?1:0;
    return setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&value,sizeof(value))==0;
}
int Socket::fd()const noexcept{
    return listenfd_;
}


