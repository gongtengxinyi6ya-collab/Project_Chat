#include "Buffer.h"

Buffer::Buffer():readerIndex_(0),writerIndex_(0){}

size_t Buffer::readableBytes() const{
    return writerIndex_-readerIndex_;
}
size_t Buffer::writeableBytes() const{
    buffer_.size()-writerIndex_;
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
void 