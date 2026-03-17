#include "until.h"

void setNonBlocking(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    if(flags<0){
        throw std::runtime_error("fcntl failed");

    }
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}