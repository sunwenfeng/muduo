## muduo分析
### 整体流程
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
### 主要的类
上述过程中，主流程涉及到以下几个类：TcpServer,Acceptor,TcpConnection,Channel,EventLoop,Poller。其他都是一些辅助类。  
#### TcpServer
* TcpServer用于建立Tcp服务器，生命期由用户控制。通过unique_ptr通过Acceptor来管理监听套接字；服务器还需要知道建立的所有的socket连接，所以TcpServer还包含一个TcpConnection的指针(shared_ptr)集合(ConnectionMap)。  
* TcpServer::start()调用Acceptor::listen()
* TcpServer::newConnection赋给Acceptor的回调函数newConnectionCallback_，生成TcpConnection对象管理已连接套接字  

TcpServer中定义的回调函数有：

| 回调函数 | 赋值  | 作用 |
| :------------------------ |:--------------------| :----------------------|
| connectionCallback_    | 用户定义      | 建立新连接之后调用 |
| messageCallback_       | 用户定义          |   收到消息后执行业务逻辑 |

#### Acceptor
* Acceptor用户管理监听套接字，生命期由TcpServer控制，TcpServer的构造函数中生成。Acceptor通过Socket对象管理监听socket（RAII），还包含一个channel，这个channel的生命期由Acceptor管理。还包括一个所属EventLoop的指针
* 构造函数执行套接字的socket()和bind()操作，并设置回调函数的具体调用
* Acceptor::listen()执行套接字的listen(),调用Channel::enableReading()将监听套接字的read事件加入Poller的关注列表
* Acceptor::handleRead()执行套接字的accept，并调用回调函数newConnectionCallback_  

Acceptor的回调函数有：

| 回调函数 | 赋值  | 作用 |
| :--------------------------- |:-----------------------------| :---------------------------------------------:|
| newConnectionCallback_    | TcpServer::newConnection      | accept返回后创建TcpConnection对象管理已连接描述符 |
 #### TcpConnection
 * TcpConnection用于管理已连接描述符，在Acceptor的newConnectionCallback_回调中创建，并用shared_ptr管理，用unique_ptr管理channel，用unique_ptr通过RAII管理已连接描述符。 
 * 构造函数建立channel对象，并设置channel的回调函数
 * TcpConnection::connectEstablished()调用channel::enableReading()将已连接描述符加入Poller的关注列表中，并且调用connectionCallback_
 * TcpConnection::handleRead()：描述符可读之后read，读成功调用messageCallback_，失败调用handleClose()或handleError()  
 
 TcpConnection的回调函数有：  
 
 | 回调函数 | 赋值  | 作用 |
| :------------------------------------ |:----------------------------------| :-----------:|
| connectionCallback_    | TcpServer:: connectionCallback_      |  |
| messageCallback_    | TcpServer:: messageCallback_    |  |
| writeCompleteCallback_    | TcpServer:: writeCompleteCallback_     |  |
| highWaterMarkCallback_    |      |  |
| closeCallback_    | TcpServer::removeConnection    |  |

#### Channel
* Channel起一个桥接作用，EventLoop作为事件驱动器，拥有Poller进行IO复用；而TcpServer通过Acceptor和TcpConnection管理Tcp连接。通过channel将这两部分连接起来，TcpServer接收到一个新连接之后，通过channel将这个新连接的socket描述符加入到Poller的关注列表。而EventLoop一直在执行循环，通过Poller监听事件，在有事件发生时再通过channel通知TcpServer进行处理。
* 每个channel对象只负责一个文件描述符的IO事件分发，但不拥有这个事件描述符。同时channel并不能单独存在，一般是其他类的成员，生命期也就由其所属的类来管理。如Acceptor有一个channel对象以及一个socket对象，channel负责这个socket对象的IO事件分发，但并不拥有，同时他的生命期由Acceptor管理。另外一个用到channel的类就是TcpConnection类。  

Channel的回调函数：  
只有两个类用到了Channel，所以这两个类中的channel对象的回调函数的赋值方式不一样。也正是通过这种方式，channel在分发事件的时候能够区分是监听套接字还是已连接套接字。  

 | 回调函数 | TcpConnection赋值方式  | 作用 |
| :------------------- |:------------------------| :---------------------------------------------------------------------|
| readCallback_     | TcpConnection::handleRead     | 已连接描述符有事件发生时，channel::handleEvent()根据不同的就绪事件调用 |
| writeCallback_    | TcpConnection::handleWrite   |  |
| closeCallback_    | TcpConnection::handleClose    |  |
| errorCallback_    | TcpConnection::handleError     |  |  

 | 回调函数 | Acceptor赋值方式  | 作用 |
| :------------------- |:----------------------------------| :-----------------------------|
| readCallback_     | Acceptor::handleRead    | 监听描述符可写，也就是接收到新连接之后调用 |

#### EventLoop
* EventLoop是一个事件驱动器，在单线程Reactor中，由用户定义，生命期由用户控制。通过unique_ptr管理一个Poller用于IO复用。
* 每个IO线程只能有一个EventLoop
* EventLoop::loop() 执行EventLoop循环，首先调用Poller::poll获得当前就绪的描述符保存在activeChannels_中，然后对activeChannels_中的每个channel执行channel::handleEvent()处理不同的就绪事件。

#### Poller
* Poller类用于IO复用，muduo中有两种实现方式：一种是Poll，一种是LT模式的Epoll，一般是EventLoop的成员，生命期由其管理。
* 用map<socket描述符, Channel*> ChannelMap保存当前Poller关心的描述符。
* Poller::updateChannel用于更新Poller监听的套接字以及监听事件。调用关系为：
Acceptor::listen()/TcpConnection::connectEstablished()-->Channel:: enableReading()-->Channel::update()-->EventLoop::updateChannel()-->Poller::updateChannel

***
### 建立连接
基于上面的分析，muduo建立连接的过程为  

![建立连接](https://github.com/sunwenfeng/muduo/raw/master/建立连接.png)  
首先用户定义TcpServer的时候就定义了Acceptor对象，并且执行了监听套接字的socket()和bind()，然后TcpServer::start()执行listen()，并将监听套接字的read事件写入到Poller的关注的描述符列表中。接下来就是上图所示的过程：  

1. 客户端发起连接，服务端Poller检测到可写事件，调用channel::handleEvent()
2. 由于是可写事件，channel调用readCallback_回调函数，对应Acceptor::handleRead  
3. Acceptor::handleRead执行accept，然后执行newConnectionCallback_回调，对应TcpServer::newConnection。
4. TcpServer::newConnection创建TcpConnection对象，并调用TcpConnection::connectEstablished
5. 在TcpConnection::connectEstablished中，将已连接描述符加入Poller的关注列表中，并且调用connectionCallback_
***
### 接收数据
接收数据的流程和上述建立连接的过程基本一样，只不过处理不再是Acceptor类进行处理，而是TcpConncetion。  

在建立连接的过程中已经将已连接描述符加入监听。  
1.loop()的Poller检测得到可写事件，调用channel::handleEvent()
2.由于是可写事件，channel调用readCallback_回调函数，对应TcpConnection::handleRead，对应
3.TcpConnection::handleRead读取数据，并且调用读成功调用messageCallback_回调进行处理，messageCallback_对应TcpServer:: messageCallback_
4.TcpServer:: messageCallback_由用户自定义，做到了业务逻辑和网络模型相独立。

***
### 断开连接
 ![断开连接](https://github.com/sunwenfeng/muduo/raw/master/断开连接.png)




