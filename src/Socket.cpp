#include "Socket.h"

Socket::Socket(){
    listenfd_=socket(AF_INET,SOCK_STREAM,0);
    if(listenfd_<0)
        throw::std::runtime_error("socket() failed");
    setNonBlocking(listenfd_);

}

Socket::~Socket(){
    close(listenfd_);
}
//绑定端口
void Socket::bind(int port){
    int opt=1;
    if(::setsockopt(listenfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
    {//允许服务器重启时重新绑定端口
        throw std::runtime_error("setsockopt filed");
    }
    sockaddr_in m_addr;
    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sin_family=AF_INET;
    m_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    m_addr.sin_port=htons(port);
    if(::bind(listenfd_,(sockaddr*)&m_addr,sizeof(m_addr))==-1){
        throw::std::runtime_error("bind() failed");
    }
}
//监听
void Socket::listen()
{
    if(::listen(listenfd_,10)<0){
        throw::std::runtime_error("listen() failed");
    }
}

//接收客户端连接
int Socket::accept(){
    sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    socklen_t len=sizeof(client_addr);
    int clientfd_=::accept(listenfd_,(sockaddr*)&client_addr,&len);
    if(clientfd_<0){
        // EAGAIN / EWOULDBLOCK is expected when using non-blocking sockets
        // and there are no more pending connections on the listen socket.
        if(errno==EAGAIN||errno==EWOULDBLOCK)
            return -1;

        std::cerr << "accept errno=" << errno << std::endl;
        throw std::runtime_error("accept() failed");
    }
    return clientfd_;

}

int Socket::fd(){
    return listenfd_;
}


