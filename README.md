## muduo分析
在一个完整的网络框架中，需要考虑网络socket事件，信号，定时。muduo采用one loop per thread + thread pool的方式实现。首先只分析单个Reactor下
网络socket事件的实现。也就是服务器的监听以及新连接的处理都在reactor线程进行。

以echo为例从用户的角度来看：
```C++
int main()
{
    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(2007);
    TcpServer server(&loop, listenAddr);
    server.setMessageCallback(onMessage);
    server.start();
    loop.loop();
}
```
1. 初始化一个EventLoop作为reactor驱动器。
2. 初始化一个TcpServer并设置业务逻辑处理回调函数。定义TcpServer时定义了Acceptor对象，并执行了服务端套接字的socket()和bind()操作。
3. 调用TcpServer::start()，执行服务端套接字的listen()，并将服务端套接字的read事件写入到Poller（处理IO复用）的关注的描述符列表中。
4. 调用EventLoop::loop()进行事件循环。loop()函数调用Poller获取当前就绪的描述符，也就是channel，然后调用channel::handleEvent()根据不同的就绪事件
调用不同的回调函数进行处理。
