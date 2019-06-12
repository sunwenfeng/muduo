// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/poller/PollPoller.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Channel.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
        : Poller(loop)
{
}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{//poller的核心函数，调用poll返回就绪的描述符
  // XXX pollfds_ shouldn't change
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0)  //有就绪描述符
  {
    LOG_TRACE << numEvents << " events happened";
    fillActiveChannels(numEvents, activeChannels);
  }
  else if (numEvents == 0)  //没有就绪描述符
  {
    LOG_TRACE << " nothing happened";
  }
  else
  {//出错
    if (savedErrno != EINTR)
    {
      errno = savedErrno;
      LOG_SYSERR << "PollPoller::poll()";
    }
  }
  return now;
}

void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
//处理就绪的描述符
{
  for (PollFdList::const_iterator pfd = pollfds_.begin();
       pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    if (pfd->revents > 0)//找出就绪的描述符，并将描述符写入到activeChannels
    {
      --numEvents;
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);//找到就绪描述符对应的channel
      assert(ch != channels_.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel);
    }
  }
}

void PollPoller::updateChannel(Channel* channel)//修改监听描述符和对应的监听事件
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0)  //新的channel，也就是新的描述符
  {
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);   //将新描述符加入监听描述符集合尾部
    int idx = static_cast<int>(pollfds_.size())-1;
    channel->set_index(idx);
    channels_[pfd.fd] = channel;
  }
  else
  {
    // update existing one，更新已有的描述符
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx]; //从监听描述符集合中找到该描述符
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.fd = channel->fd();  //更新监听事件
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    if (channel->isNoneEvent())  //不关心任何事件
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;
    }
  }
}

void PollPoller::removeChannel(Channel* channel)   //某个描述符删除，则删除对应的channel
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());
  size_t n = channels_.erase(channel->fd());
  assert(n == 1); (void)n;
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1)
  {
    pollfds_.pop_back();
  }
  else
  {
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

