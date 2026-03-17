#include "Acceptor.h"

Acceptor::Acceptor(EventLoop* loop,int port):loop_(loop),listenSocket_()
{   
    listenSocket_.bind(port);
    listenSocket_.listen();


    channel_=new Channel(loop_,listenSocket_.fd());
    channel_->setReadCallback([this](){handleRead();});
    channel_->enableReading();

    loop_->addChannel(channel_);
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