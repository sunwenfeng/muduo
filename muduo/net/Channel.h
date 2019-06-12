// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/Timestamp.h"

#include <functional>
#include <memory>

namespace muduo
{
    namespace net
    {

        class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
        class Channel : noncopyable
        {
        public:
            typedef std::function<void()> EventCallback;
            typedef std::function<void(Timestamp)> f;

            Channel(EventLoop* loop, int fd);
            ~Channel();

            void handleEvent(Timestamp receiveTime);

            void setReadCallback(ReadEventCallback cb)   //绑定回调函数
            { readCallback_ = std::move(cb); }
            void setWriteCallback(EventCallback cb)
            { writeCallback_ = std::move(cb); }
            void setCloseCallback(EventCallback cb)
            { closeCallback_ = std::move(cb); }
            void setErrorCallback(EventCallback cb)
            { errorCallback_ = std::move(cb); }

            /// Tie this channel to the owner object managed by shared_ptr,
            /// prevent the owner object being destroyed in handleEvent.
            /*
             * 用shared_ptr来把channel所属的对象绑定到channel上，以防止channel在执行handleEvent时析构channel所属对象
             *
             * */
            void tie(const std::shared_ptr<void>&);

            int fd() const { return fd_; }
            int events() const { return events_; }
            void set_revents(int revt) { revents_ = revt; } // used by pollers  pollers使用，将描述符的就绪事件写入对应channel的就绪事件
            // int revents() const { return revents_; }
            bool isNoneEvent() const { return events_ == kNoneEvent; }

            void enableReading() { events_ |= kReadEvent; update(); }
            void disableReading() { events_ &= ~kReadEvent; update(); }
            void enableWriting() { events_ |= kWriteEvent; update(); }
            void disableWriting() { events_ &= ~kWriteEvent; update(); }
            void disableAll() { events_ = kNoneEvent; update(); }
            bool isWriting() const { return events_ & kWriteEvent; }
            bool isReading() const { return events_ & kReadEvent; }

            // for Poller
            int index() { return index_; }
            void set_index(int idx) { index_ = idx; }

            // for debug
            string reventsToString() const;
            string eventsToString() const;

            void doNotLogHup() { logHup_ = false; }

            EventLoop* ownerLoop() { return loop_; }
            void remove();

        private:
            static string eventsToString(int fd, int ev);

            void update();
            void handleEventWithGuard(Timestamp receiveTime);

            static const int kNoneEvent;
            static const int kReadEvent;
            static const int kWriteEvent;

            EventLoop* loop_;     //channel对应的eventloop
            const int  fd_;       //channel对应的描述符
            int        events_;
            int        revents_; // it's the received event types of epoll or poll
            int        index_; // used by Poller.当前channel对应的描述符在poller中监听描述符集合中的的索引
            bool       logHup_;

            std::weak_ptr<void> tie_;
            bool tied_;                   //是否绑定
            bool eventHandling_;
            bool addedToLoop_;
            ReadEventCallback readCallback_;
            EventCallback writeCallback_;
            EventCallback closeCallback_;
            EventCallback errorCallback_;
        };

    }  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CHANNEL_H
