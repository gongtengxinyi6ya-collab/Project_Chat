#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
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
    if(listening_){
        return ;
    }
    listenSocket_.bind(port_);
    listenSocket_.listen();

    channel_=std::make_unique<Channel>(loop_,listenSocket_.fd());
    channel_->setReadCallback(std::bind(&Acceptor::handleRead,this));
    channel_->enableReading();
    listening_=true;
}

void Acceptor::stop(){
    if(!listening_){
        return;
    }
    if(!loop_->isInLoopThread()){
        loop_->runInLoop([this](){
            channel_->disableAll();
            loop_->removeChannel(listenSocket_.fd());
            listening_=false;
        });
    }
    else{
        channel_->disableAll();
        loop_->removeChannel(listenSocket_.fd());
        listening_=false;
    }

}