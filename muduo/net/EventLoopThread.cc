// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
        : loop_(NULL),
          exiting_(false),
          thread_(std::bind(&EventLoopThread::threadFunc, this), name),
          mutex_(),
          cond_(mutex_),
          callback_(cb)
{//创建eventloopthread对象，调用thread的构造函数，创建一个thread对象，但没有创建新线程。
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop()
// 必须调用startLoop才会创建新线程，主线程继续执行startLoop并阻塞在条件变量上，在新线程执行threadFunc，
// 新线程创建完eventloop对象后通知主线程，主线程不再阻塞。
{
  assert(!thread_.started());
  thread_.start();

  EventLoop* loop = NULL;
  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      cond_.wait();
    }
    loop = loop_;
  }

  return loop;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();                 //创建eventloop对象后，通过条件变量唤醒startloop
  }

  loop.loop();
  //assert(exiting_);
  MutexLockGuard lock(mutex_);
  loop_ = NULL;
}

