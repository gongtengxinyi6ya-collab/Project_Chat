#include "TcpServer.h"

TcpServer::TcpServer(EventLoop* loop,int port)
:loop_(loop),acceptor_(loop_,port){
    acceptor_.setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,std::placeholders::_1));
    threadPool_ = std::make_unique<ThreadPool>();
}

TcpServer::~TcpServer(){
    // Ensure remaining connections are cleaned up if server is destroyed.
    connections_.clear();
}

void TcpServer::start(){
    loop_->loop();
}

void TcpServer::newConnection(int fd){
    //检测fd重复
    if(connections_.find(fd)!=connections_.end()){
        std::cerr<<"fd already exists!"<<std::endl;
        return;
    }

    auto newconnection = std::make_unique<TcpConnection>(loop_,fd,threadPool_.get(),this);
    std::cout<<"new connection fd:"<<fd<<std::endl;

    // 先设置回调再把 unique_ptr 放入容器，避免悬空引用
    newconnection->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));
    newconnection->setMessageCallback(std::bind(&TcpServer::onMessage,this,std::placeholders::_1,std::placeholders::_2));

    connections_[fd] = std::move(newconnection);
}

void TcpServer::removeConnection(int fd){
    auto it=connections_.find(fd);
    if(it!=connections_.end()){
        connections_.erase(it);
        std::cout<<"connection closed fd:"<<fd<<std::endl;
    }

}

void TcpServer::onMessage(int fd,const std::string& msg){
    //转发消息给其他客户端
    for(auto& pair:connections_){
        if(pair.first!=fd){
            pair.second->send(msg);
        }
    }
}