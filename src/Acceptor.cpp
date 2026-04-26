#include "Acceptor.h"

Acceptor::Acceptor(EventLoop* loop,int port):port_(port),listenSocket_(),loop_(loop),channel_(nullptr)
{   
    
}

void Acceptor::handleRead(){
    while(true){
        int client=listenSocket_.accept();

        if(client<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK)
                break;
            else
                return;

        }
        setNonBlocking(client);//客户端非阻塞

        if(newConnectionCallback_)
            newConnectionCallback_(client);
    }
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb){
    newConnectionCallback_=std::move(cb);
}

void Acceptor::listen(){
    listenSocket_.bind(port_);
    listenSocket_.listen();

    channel_=new Channel(loop_,listenSocket_.fd());
    channel_->setReadCallback(std::bind(&Acceptor::handleRead,this));
    channel_->enableReading();
   
}