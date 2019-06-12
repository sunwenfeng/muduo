// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
        : loop_(CHECK_NOTNULL(loop)),
          ipPort_(listenAddr.toIpPort()),
          name_(nameArg),
          acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
          threadPool_(new EventLoopThreadPool(loop, name_)),
          connectionCallback_(defaultConnectionCallback),
          messageCallback_(defaultMessageCallback),
          nextConnId_(1)
{
  acceptor_->setNewConnectionCallback(
          std::bind(&TcpServer::newConnection, this, _1, _2));
  // 创建acceptor对象，定义服务端描述符，并将TcpServer::newConnection设为其回调函数
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (auto& item : connections_)
  {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
  }
}

void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    threadPool_->start(threadInitCallback_);

    assert(!acceptor_->listenning());
    loop_->runInLoop(
            std::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)//(已连接描述符，客户端地址)
{//所以，在新连接到来之后，创建TcpConnection对象进行管理
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  //创建TcpConnection对象
  connections_[connName] = conn;                            //加入ConnectionMap
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
          std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe   设置回调函数
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));     //在新连接中调用connectionCallback_回调函数
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());     //把关闭的TcpConnection从map中删除
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop(
          std::bind(&TcpConnection::connectDestroyed, conn));   //queueLoop增长TcpConnection的生命期？？
}

