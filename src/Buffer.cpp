#include "Buffer.h"

Buffer::Buffer():buffer_(kCheapPrepend+1024),readerIndex_(kCheapPrepend),writerIndex_(kCheapPrepend){}

size_t Buffer::readableBytes() const{
    return writerIndex_-readerIndex_;
}
size_t Buffer::writeableBytes() const{
    return buffer_.size()-writerIndex_;
}
size_t Buffer::prependableBytes() const{
    return readerIndex_;
}
const char* Buffer::peek() const{
    return buffer_.data()+readerIndex_;
}
char* Buffer::beginWrite(){
    return buffer_.data()+writerIndex_;
}
void Buffer::hasWritten(size_t n){
    assert(n<=writeableBytes());
    writerIndex_+=n;
}

void Buffer::retrieve(size_t len){
    if(len<readableBytes())
        readerIndex_+=len;
    else{
        retrieveAll();
    }
}
void Buffer::retrieveUntil(const char* end){
    assert(peek()<=end);
    assert(end<=beginWrite());
    retrieve(end-peek());
}
void Buffer::retrieveAll(){
    readerIndex_=kCheapPrepend;
    writerIndex_=kCheapPrepend;
}
std::string Buffer::retrieveAsString(size_t len){
    assert(len<=readableBytes());
    len=std::min(len,readableBytes());
    std::string result(peek(),len);
    retrieve(len);
    return result;
}

void Buffer::ensureWritableBytes(size_t len){
    //优先搬移，其次扩容
    if(writeableBytes()>=len)
        return ;
    else if((writeableBytes()+prependableBytes()-kCheapPrepend)>=len)
    {
        size_t readableBytes_old=readableBytes();
        std::memmove(buffer_.data()+kCheapPrepend,peek(),readableBytes());
        readerIndex_=kCheapPrepend;
        writerIndex_=kCheapPrepend+readableBytes_old;
    }
    else{
        buffer_.resize(writerIndex_+len);
    }
}

void Buffer::append(const char* data,size_t len){
    ensureWritableBytes(len);
    std::copy(data,data+len,beginWrite());
    hasWritten(len);
}

const char* Buffer::findEOL()const{
    const char* res=(const char*)memchr(peek(),'\n',readableBytes());
    if(!res)
        return nullptr;
    else
        return res;
}
ssize_t Buffer::readFd(int fd,int* savedErrno){
    iovec vec[2];
    char extrabuf[65536];
    vec[0].iov_base=beginWrite();
    vec[0].iov_len=writeableBytes();
    vec[1].iov_base=extrabuf;
    vec[1].iov_len=sizeof(extrabuf);
    size_t writeable=writeableBytes();
    ssize_t n=readv(fd,vec,2);
    if(n<0){
        if(savedErrno)
            *savedErrno=errno;
        return n;
    }
    else if ((size_t)n <= writeable) {
    
        hasWritten(n);
    }
    else {
      
        hasWritten(writeable);
        append(extrabuf, n - writeable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd,int* savedErrno){
    ssize_t n=::send(fd,peek(),readableBytes(),MSG_NOSIGNAL);
    if(n<0){
            if(savedErrno)
                *savedErrno=errno;
    }
    else if(n>0){
        retrieve(n);
    }
    return n;
}

//4字节长度前缀协议
u_int32_t Buffer::peekUInt32() const{
    assert(readableBytes()>=4);
    const char* p=peek();
    u_int32_t x=0;
    memcpy(&x,p,sizeof(x));
    return ntohl(x);

}

void Buffer::retrieveUInt32(){
    assert(readableBytes()>=4);
    retrieve(4);
}
void Buffer::appendUint32(uint32_t x){
    uint32_t be=htonl(x);
    append(reinterpret_cast<const char *>(&be),sizeof(be));
}