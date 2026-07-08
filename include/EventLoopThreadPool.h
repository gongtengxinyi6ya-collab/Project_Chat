#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

//管理多个EventLoopThread，提供获取EventLoop的接口
class EventLoopThreadPool{
public:
    EventLoopThreadPool(EventLoop* baseLoop);
    ~EventLoopThreadPool();
    void start();//启动线程池，创建EventLoopThread对象，并启动线程
    EventLoop* getNextLoop();//获取下一个EventLoop对象，采用轮询的
    void setThreadNum(int numThreads);//设置线程池中线程的数量

    void stop();//停止线程池中所有io loop
    bool started()const{return started_;}
private:
    EventLoop* baseLoop_;//主线程的EventLoop对象指针
    bool started_;//标志线程池是否已经启动
    int numThreads_;//线程池中线程的数量
    size_t next_;//轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;//线程池中的EventLoopThread对象列表
    std::vector<EventLoop*> loops_;//线程池中EventLoop对象的指针列表
};