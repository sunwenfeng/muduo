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
4. 调用EventLoop::loop()进行事件循环。loop()函数调用Poller获取当前就绪的描述符，然后调用channel::handleEvent()根据不同的就绪事件
调用不同的回调函数进行处理。  

    此时Poller关注的描述符只有监听描述符，当监听描述符可写之后，表明收到了客户端的连接，channel::handleEvent()进行处理，由于是描述符可写事件，
调用写回调函数readCallback。在这个回调函数中，创建TcpConnection对象管理新连接，并把这个新连接的描述符加入到
Poller关注的描述符列表中。  

这样，不论是客户端请求监听描述符的新连接，还是在已连接描述符上的读写事件，EventLoop事件驱动器都能从容应对。

***
上述过程中，主流程涉及到以下几个类：TcpServer,Acceptor,TcpConnection,Channel,EventLoop,Poller。其他都是一些辅助类。  
TcpServer用于建立Tcp服务器，生命期由用户控制。通过unique_ptr通过Acceptor来管理监听套接字；服务器还需要知道建立的所有的socket连接，所以TcpServer还包含一个TcpConnection的指针(shared_ptr)集合(ConnectionMap)。
TcpServer中定义的回调函数有：

表头1  | 表头2|
--------- | --------|
表格单元  | 表格单元 |
表格单元  | 表格单元 |

| 表头1  | 表头2|
| ---------- | -----------|
| 表格单元   | 表格单元   |
| 表格单元   | 表格单元   |

### 对齐
表格可以指定对齐方式

| 回调函数 | 赋值  | 作用 |
| :--------------------: |:------------:| :------------------:|
| connectionCallback_    | 用户定义      | 建立新连接之后调用 |
| messageCallback_       | 用户定义          |   收到消息后执行业务逻辑 |





