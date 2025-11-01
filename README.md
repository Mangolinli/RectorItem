

## 核心组件

1. **EventLoop（事件循环）**：每个线程一个事件循环，负责监听和分发事件。
2. **Poller（事件监听器）**：封装epoll，负责监听文件描述符上的事件，并上报给EventLoop。
3. **Channel（通道）**：每个连接对应一个Channel，负责注册文件描述符和回调函数。
4. **Acceptor（接收器）**：负责监听新的连接，并分发新的连接给EventLoop。
5. **TcpConnection（TCP连接）**：代表一个已建立的连接，负责处理该连接上的读写事件。
6. **ThreadPool（线程池）**：一组工作线程，负责处理业务逻辑。
7. **HttpContext（HTTP上下文）**：解析HTTP请求，并生成HTTP响应。
8. **TimerQueue（定时器队列）**：管理定时任务，用于处理超时连接。

## 类设计

### 1. EventLoop类

+ 核心成员：
  + `Poller* poller_`：事件监听器。
  + `ChannelList activeChannels_`：活跃的事件通道列表。
  + `bool looping_`：是否处于事件循环中。
  + `int wakeupFd_`：用于唤醒事件循环的文件描述符（用于跨线程通信）。
  + `Channel wakeupChannel_`：用于唤醒的通道。
+ 核心方法：
  + `loop()`：开始事件循环。
  + `updateChannel(Channel* channel)`：更新通道监听的事件。
  + `removeChannel(Channel* channel)`：移除通道。
  + `wakeup()`：唤醒事件循环。
  + `handleRead()`：处理唤醒事件。

### 2. Poller类（抽象基类，具体实现为EPollPoller）

+ 核心成员：
  + `int epollfd_`：epoll文件描述符。
  + `std::vector<struct epoll_event> events_`：用于存放epoll返回的事件。
+ 核心方法：
  + `poll(int timeoutMs, ChannelList* activeChannels)`：监听事件，并将活跃通道填入activeChannels。
  + `updateChannel(Channel* channel)`：更新通道。
  + `removeChannel(Channel* channel)`：移除通道。

### 3. Channel类

+ 核心成员：
  + `int fd_`：负责的文件描述符。
  + `EventLoop* loop_`：所属的事件循环。
  + `uint32_t events_`：监听的事件。
  + `uint32_t revents_`：返回的事件。
  + `ReadEventCallback readCallback_`：读事件回调。
  + `WriteEventCallback writeCallback_`：写事件回调。
  + `CloseEventCallback closeCallback_`：关闭事件回调。
+ 核心方法：
  + `handleEvent()`：处理事件，根据revents调用相应的回调。
  + `enableReading()`：启用读事件。
  + `enableWriting()`：启用写事件。
  + `disableWriting()`：禁用写事件。

### 4. Acceptor类

+ 核心成员：
  + `EventLoop* loop_`：所属事件循环。
  + `int listenfd_`：监听套接字。
  + `Channel acceptChannel_`：监听套接字的通道。
  + `NewConnectionCallback newConnectionCallback_`：新连接回调。
+ 核心方法：
  + `listen()`：开始监听。
  + `handleRead()`：处理读事件（接受新连接）。

### 5. TcpConnection类

+ 核心成员：
  + `EventLoop* loop_`：所属事件循环。
  + `int sockfd_`：连接套接字。
  + `Channel channel_`：连接的通道。
  + `Buffer inputBuffer_`：输入缓冲区。
  + `Buffer outputBuffer_`：输出缓冲区。
  + `HttpContext context_`：HTTP上下文。
  + `CloseCallback closeCallback_`：关闭回调。
+ 核心方法：
  + `send(const void* data, size_t len)`：发送数据。
  + `send(Buffer* buffer)`：发送缓冲区中的数据。
  + `shutdown()`：关闭连接。
  + `handleRead()`：处理读事件。
  + `handleWrite()`：处理写事件。
  + `handleClose()`：处理关闭事件。

### 6. ThreadPool类

+ 核心成员：
  + `std::vector<std::thread> threads_`：工作线程组。
  + `std::queue<std::function<void()>> tasks_`：任务队列。
  + `std::mutex mutex_`：互斥锁。
  + `std::condition_variable cond_`：条件变量。
  + `bool stop_`：是否停止。
+ 核心方法：
  + `enqueue(F&& f, Args&&... args)`：添加任务。
  + `worker()`：工作线程函数。

### 7. HttpContext类

+ 核心成员：
  + `HttpRequest request_`：当前解析的请求。
  + `HttpParseState state_`：解析状态。
+ 核心方法：
  + `parseRequest(Buffer* buffer)`：解析HTTP请求。
  + `parseLine(Buffer* buffer, std::string& line)`：解析一行。
  + `parseRequestLine(const std::string& line)`：解析请求行。
  + `parseHeaders(const std::string& line)`：解析头部。
  + `parseBody(const std::string& line)`：解析体。

### 8. TimerQueue类

+ 核心成员：
  + `std::priority_queue<TimerNode> timers_`：定时器最小堆（按超时时间排序）。
  + `Channel timerChannel_`：用于定时器的通道（通常使用timerfd）。
+ 核心方法：
  + `addTimer(std::function<void()> cb, int timeout)`：添加定时器。
  + `handleRead()`：处理定时器事件（检查超时的定时器并执行回调）。

## 工作流程

1. **启动服务器**：
   + 创建`EventLoop`。
   + 创建`Acceptor`并注册到`EventLoop`。
   + 创建`ThreadPool`。
   + 启动事件循环。
2. **接受新连接**：
   + `Acceptor`监听到新连接，调用`accept`得到`connfd`。
   + 创建`TcpConnection`对象，并设置回调函数。
   + 将`connfd`注册到`EventLoop`。
3. **处理读事件**：
   + 当`connfd`可读时，`TcpConnection`的`handleRead`被调用，将数据读入`inputBuffer`。
   + 将`inputBuffer`交给`HttpContext`解析，解析出`HttpRequest`。
   + 根据`HttpRequest`生成`HttpResponse`，并调用`send`发送响应。
4. **处理写事件**：
   + 当`connfd`可写时，`TcpConnection`的`handleWrite`被调用，将`outputBuffer`中的数据发送出去。
5. **处理关闭事件**：
   + 当对端关闭连接时，`TcpConnection`的`handleClose`被调用，然后关闭本端连接，并清理资源。
6. **定时器处理**：
   + 定时检查是否有超时的连接，如果有则关闭。