#include "TcpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "TcpConnection.h"

TcpServer::TcpServer(EventLoop* loop,int port,const AppConfig& config)
:baseloop_(loop),acceptor_(baseloop_,port),threadNum_(config.server().ioThreads),started_(false),config_(config){
    iothreadPool_ = std::make_unique<EventLoopThreadPool>(baseloop_);
    acceptor_.setNewConnectionCallback([this](int fd){
        newConnection(fd);
    });
    threadPool_ = std::make_unique<ThreadPool>();
    imService_ = std::make_unique<im::Imservice>(1,config_.im());
    imService_->setSendToConnKey([this](im::Imservice::ConnKey key,const std::string& payload){
            auto it=connections_.find(key);
            if(it!=connections_.end()){
                it->second->send(payload);
                return true;
            }
            return false;
});
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
//从Acceptor接收到新连接，在EventLoopThreadPool中选择一个IO线程，创建TcpConnection对象，
//再投递到baseloop保存到connections_中
void TcpServer::newConnection(int fd){
    //检测fd重复
    if(connections_.find(fd)!=connections_.end()){
        LOG_ERROR("Duplicate fd "+std::to_string(fd)+" received in newConnection, closing it");
        return;
    }
    EventLoop* ioloop=iothreadPool_->getNextLoop();
    ioloop->runInLoop([this,fd,ioloop](){
        auto conn=std::make_shared<TcpConnection>(ioloop,fd,threadPool_.get(),this,config_);
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
    LOG_INFO("New connection fd: " + std::to_string(fd)+"assigned to ioloop" );
}

void TcpServer::removeConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    auto it=connections_.find(fd);
    if(it!=connections_.end()){
        auto ioloop=conn->getLoop();
        connections_.erase(it);
        LOG_INFO("Connection removed fd: " + std::to_string(fd));
        ioloop->queueInLoop([conn](){
            conn->connectionDestroyed();
        });
    }
    imService_->onDisconnect(conn);//通知IM业务连接断开，清理状态

}

void TcpServer::onMessage(const std::shared_ptr<TcpConnection>& conn,const std::string& msg){
    //转发给IM业务对象处理
    imService_->onMessage(conn,msg);    
}
void TcpServer::setThreadNum(int numThreads){
    threadNum_=numThreads;
}

void TcpServer::addConnectionInBaseLoop(const std::shared_ptr<TcpConnection>& conn){
    int fd=conn->fd();
    connections_[fd]=std::move(conn);
}