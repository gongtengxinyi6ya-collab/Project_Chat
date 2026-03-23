#include "TcpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "TcpConnection.h"

TcpServer::TcpServer(EventLoop* loop,int port)
:baseloop_(loop),acceptor_(baseloop_,port),threadNum_(0),started_(false){
    iothreadPool_ = std::make_unique<EventLoopThreadPool>(baseloop_);
    acceptor_.setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,std::placeholders::_1));
    threadPool_ = std::make_unique<ThreadPool>();
}

TcpServer::~TcpServer(){
    // Ensure remaining connections are cleaned up if server is destroyed.
    connections_.clear();
}

void TcpServer::start(){
    if(started_)
        return;
    started_=true;
    iothreadPool_->setThreadNum(threadNum_);
    iothreadPool_->start();
    acceptor_.listen();
}

void TcpServer::newConnection(int fd){
    //检测fd重复
    if(connections_.find(fd)!=connections_.end()){
        std::cerr<<"fd already exists!"<<std::endl;
        return;
    }
    EventLoop* ioloop=iothreadPool_->getNextLoop();
    ioloop->runInLoop([this,fd,ioloop](){
        auto conn=std::make_shared<TcpConnection>(ioloop,fd,threadPool_.get(),this);
        conn->setMessageCallback([this](std::shared_ptr<TcpConnection> conn,const std::string& msg){
            baseloop_->runInLoop([this,conn,msg](){
                onMessage(conn,msg);
            });
        });
        conn->setCloseCallback([this](std::shared_ptr<TcpConnection> conn){
                int fd=conn->fd();
                baseloop_->runInLoop([this,conn](){
                    removeConnectionInBaseLoop(conn);
                });
        });
        conn->connectionEstablished();
        baseloop_->runInLoop([this,conn](){
            addConnectionInBaseLoop(conn);
        });
    });
}

void TcpServer::removeConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    auto it=connections_.find(fd);
    if(it!=connections_.end()){
        auto ioloop=conn->getLoop();
        connections_.erase(it);
        std::cout<<"connection closed fd:"<<fd<<std::endl;
        ioloop->queueInLoop([conn](){
            conn->connectionDestroyed();
        });
    }

}

void TcpServer::onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& msg){
    //转发消息给其他客户端
    for(auto& pair:connections_){
        if(pair.second!=conn){
            pair.second->send(msg);
        }
    }
}
void TcpServer::setThreadNum(int numThreads){
    threadNum_=numThreads;
}

void TcpServer::addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    connections_[fd]=std::move(conn);
}