#pragma once //防止重复包含

#include<sys/epoll.h>
#include<vector>
#include<memory>
#include<functional>
#include<unordered_map>

class Channel;
class Epoll;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop(); //开始事件循环
    void updateChannel(Channel *channel); //更新通道
    void removeChannel(Channel *channel); //移除通道
    void assertInLoopThread(){
         //断言在事件循环线程中
         if(!isInLoopThread()){
            abortNotInLoopThread(); //如果不在事件循环线程中则中止
         }
    }

    bool isInLoopThread() const {
        return threadId_ == std::this_thread::get_id(); //检查当前线程ID是否与事件循环线程ID相同
    }
private:
    void abortNotInLoopThread(); //中止程序，打印错误信息

    bool looping_; //是否正在循环
    const std::thread::id threadId_; //事件循环所在的线程ID
    std::unique_ptr<Epoll> epoll_;  //epoll实例
    std::vector<Channel*> activeChannels_;  //活跃的通道列表
};