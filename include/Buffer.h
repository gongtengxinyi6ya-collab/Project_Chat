#pragma once
#include <vector>
#include <string>
#include <assert.h>
#include <cstring>
#include <algorithm>
#include <sys/uio.h>
#include <sys/unistd.h>
//封装Buffer,缓存数据，管理读写位置并支持解析完整消息
class Buffer{
public:
    Buffer();
    //base function
    size_t readableBytes() const;//返回当前可读字节数
    size_t writeableBytes() const;//返回当前可写剩余字节数
    size_t prependableBytes() const;//返回readerIndex_前面可回收的空间
    const char* peek() const;//返回可读起点的指针
    char* beginWrite();//返回可写起点指针

    void hasWritten(size_t n);//写内存后推进writeIndex_
    void ensureWritableBytes(size_t len);//保证至少有len的可写空间
    void append(const char* data,size_t len);//把外部数据追加到buffer
    void retrieve(size_t len);//消费len字节，从可读取移走
    void retrieveAll();//清空buffer，但不释放容量
    std::string retrieveAsString(size_t len);//取出len字节并消费

    //socket for ET
    ssize_t readFd(int fd,int* savedErrno);//把fd上的数据读进buffer
    ssize_t writeFd(int fd,int * savedErrno);//把readable区写到fd

    //行协议
    const char* findEOL() const;//查找\n
    void retrieveUntil(const char* end);//消费到某个指针位置



private:
    std::vector<char> buffer_;
    size_t readerIndex_;//读位置
    size_t writerIndex_;//写位置
    static constexpr size_t kCheapPrepend=8;//预留空间，方便在头部添加数据

};
