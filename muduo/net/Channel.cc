// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
        : loop_(loop),
          fd_(fd__),
          events_(0),
          revents_(0),
          index_(-1),
          logHup_(true),
          tied_(false),
          eventHandling_(false),
          addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::update()
{
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{//channel的核心，由eventloop调用，根据revents_的值调用执行不同的函数回调
    std::shared_ptr<void> guard;
    if (tied_)
    {
        guard = tie_.lock();   //为了保证安全，由weak_ptr提升为shared_ptr，提升失败则表明不存在
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))//挂起且不可读
    {
        if (logHup_)
        {
            LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & POLLNVAL)   //不是一个打开的文件
    {
        LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL)) //错误或不可打开
    {
        if (errorCallback_) errorCallback_();
    }
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))//可读或挂起
    {
        if (readCallback_) readCallback_(receiveTime);
    }
    if (revents_ & POLLOUT)//可写
    {
        if (writeCallback_) writeCallback_();
    }
    eventHandling_ = false;
}

string Channel::reventsToString() const
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}
