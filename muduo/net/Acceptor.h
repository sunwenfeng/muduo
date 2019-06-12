// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <functional>

#include "muduo/net/Channel.h"
#include "muduo/net/Socket.h"

namespace muduo
{
    namespace net
    {

        class EventLoop;
        class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
        /*
         * accept一个新连接，并通过回调通知使用者，供TcpServer使用，生命期由其控制
         * 一个accept就是一个eventloop
         *
         *
         * */
        class Acceptor : noncopyable
        {
        public:
            typedef std::function<void (int sockfd, const InetAddress&)> NewConnectionCallback;

            Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
            ~Acceptor();

            void setNewConnectionCallback(const NewConnectionCallback& cb)
            { newConnectionCallback_ = cb; }

            bool listenning() const { return listenning_; }
            void listen();

        private:
            void handleRead();

            EventLoop* loop_;
            Socket acceptSocket_;                               // RAII, listing socket
            Channel acceptChannel_;                             // 一个channel对应一个描述符，对应acceptSocket_，观察socket可写，然后调用handleRead
            NewConnectionCallback newConnectionCallback_;
            bool listenning_;                                   //是否在监听
            int idleFd_;
        };

    }  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_ACCEPTOR_H
