#include "TimerId.h"

TimerId::TimerId():sequence_(0),loop_(nullptr)
{

}
TimerId::TimerId(uint64_t seq,EventLoop* loop):sequence_(seq),loop_(loop)
{

}