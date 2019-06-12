// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/InetAddress.h"

#include <memory>

#include <boost/any.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
    namespace net
    {

        class Channel;
        class EventLoop;
        class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
        //TcpConnection使用channel来获得socket上的IO事件，自己处理可写事件，对于可读事件通过messageCallback回调函数发送给客户
        class TcpConnection : noncopyable,
                              public std::enable_shared_from_this<TcpConnection>
        {
        public:
            /// Constructs a TcpConnection with a connected sockfd
            ///
            /// User should not create this object.
            TcpConnection(EventLoop* loop,
                          const string& name,
                          int sockfd,
                          const InetAddress& localAddr,
                          const InetAddress& peerAddr);
            ~TcpConnection();

            EventLoop* getLoop() const { return loop_; }
            const string& name() const { return name_; }
            const InetAddress& localAddress() const { return localAddr_; }
            const InetAddress& peerAddress() const { return peerAddr_; }
            bool connected() const { return state_ == kConnected; }
            bool disconnected() const { return state_ == kDisconnected; }
            // return true if success.
            bool getTcpInfo(struct tcp_info*) const;
            string getTcpInfoString() const;

            // void send(string&& message); // C++11
            void send(const void* message, int len);
            void send(const StringPiece& message);
            // void send(Buffer&& message); // C++11
            void send(Buffer* message);  // this one will swap data
            void shutdown(); // NOT thread safe, no simultaneous calling
            // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
            void forceClose();
            void forceCloseWithDelay(double seconds);
            void setTcpNoDelay(bool on);
            // reading or not
            void startRead();
            void stopRead();
            bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

            void setContext(const boost::any& context)
            { context_ = context; }

            const boost::any& getContext() const
            { return context_; }

            boost::any* getMutableContext()
            { return &context_; }

            void setConnectionCallback(const ConnectionCallback& cb)
            { connectionCallback_ = cb; }

            void setMessageCallback(const MessageCallback& cb)
            { messageCallback_ = cb; }

            void setWriteCompleteCallback(const WriteCompleteCallback& cb)
            { writeCompleteCallback_ = cb; }

            void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
            { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

            /// Advanced interface
            Buffer* inputBuffer()
            { return &inputBuffer_; }

            Buffer* outputBuffer()
            { return &outputBuffer_; }

            /// Internal use only.
            void setCloseCallback(const CloseCallback& cb)
            { closeCallback_ = cb; }

            // called when TcpServer accepts a new connection，主要执行新连接回调函数
            void connectEstablished();   // should be called only once
            // called when TcpServer has removed me from its map
            void connectDestroyed();  // should be called only once

        private:
            enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
            void handleRead(Timestamp receiveTime);     //socket可读之后的回调函数
            void handleWrite();
            void handleClose();
            void handleError();
            // void sendInLoop(string&& message);
            void sendInLoop(const StringPiece& message);
            void sendInLoop(const void* message, size_t len);
            void shutdownInLoop();
            // void shutdownAndForceCloseInLoop(double seconds);
            void forceCloseInLoop();
            void setState(StateE s) { state_ = s; }
            const char* stateToString() const;
            void startReadInLoop();
            void stopReadInLoop();

            EventLoop* loop_;
            const string name_;
            StateE state_;  // FIXME: use atomic variable    TcpConnection对象状态
            bool reading_;
            // we don't expose those classes to client.
            std::unique_ptr<Socket> socket_;            //管理的已连接描述符
            std::unique_ptr<Channel> channel_;          //通过channel管理已连接描述符上的IO
            const InetAddress localAddr_;
            const InetAddress peerAddr_;
            ConnectionCallback connectionCallback_;
            MessageCallback messageCallback_;
            WriteCompleteCallback writeCompleteCallback_;
            HighWaterMarkCallback highWaterMarkCallback_;
            CloseCallback closeCallback_;
            size_t highWaterMark_;
            Buffer inputBuffer_;                        //输入缓冲区
            Buffer outputBuffer_;                       // FIXME: use list<Buffer> as output buffer.
            boost::any context_;
            // FIXME: creationTime_, lastReceiveTime_
            //        bytesReceived_, bytesSent_
        };

        typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

    }  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TCPCONNECTION_H
