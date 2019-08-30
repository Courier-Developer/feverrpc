# Devlopment Documentation

# Things you should know

该框架采用的Socket连接均由Server监听，分别负责双方的事件调用。

由于还在开发阶段，因此暂时不做文件分割。

- feverrpc.cpp rpc主要代码
- lock.cpp 线程管理及通信

## For Client Devloper

需要通过多线程维护三个线程

- Server2Client
- Client2Server
- Client2Server - 心跳包

但其实更建议后两个使用一个线程，使用消息队列定时送入心跳包。

简单来说，启动方式如下

```C++
#include "feverrpc.cpp"
#include "lock.cpp" // 存放线程管理相关内容
#include "utils.cpp" // 存放一些RPC通信中的类，比如`Login`
#include <string>
#include <thread>
#include <chrono>
using namespace std;

int sub(int a, int b) { return a - b; }

int deal_push(push::Push p) {
    cout << p << endl;
    return 2;
}

Login ret_login_info() {
    return Login{username : "example", password : "password"};
}



int main(int argc, char const *argv[]) {

    thread _thread{[]() {
        // 负责Server2Client
        FeverRPC::Client _rpc("127.0.0.1");
        _rpc.bind("sub", sub);
        // 必须绑定login方法，服务器将会立刻调用
        _rpc.bind("login", ret_login_info);
        // 必须绑定push方法，用于服务器推送通知
        _rpc.bind("push", deal_push);
        _rpc.s2c();
    }};

    thread_guard g(_thread);
    
    // 负责Client2Server
    FeverRPC::Client rpc("127.0.0.1");
    // 第一次使用时必须调用login方法认证身份, 返回值为uid
    int uid = rpc.call<int>("login", Login{username : "uname", password : "pwod"});

    while(1){
        this_thread::sleep_for(chrono::seconds(2));
        rpc.call<int>("add", 1, 3, 5);
    }
    return 0;
}


```
### 退出时

优先退出c2s的线程，如果收到了`已在异地登录`的通知，也需自行退出c2s的线程。


## For Server Developer

服务端要注意的是，由于和线程管理一起使用，需要声明全局的ThreadManager

```C++
#include "feverrpc.cpp"
#include <chrono>
#include <string>
#include <thread>
#include "lock.cpp"
using namespace std;

ThreadManager threadmanager;

int echo_thread() {
    cout << "on [echo_thread]" << std::this_thread::get_id() << endl;
    return 0;
}

string repeat(string text, int times) {
    string ret;
    while (times--) {
        ret += text;
    }
    return ret;
}


int main(int argc, char const *argv[]) {
    while (1) {
        thread _thread{[]() {
            // 用于Server2Client
            FeverRPC::Server _rpc(threadmanager);
            _rpc.s2c();
        }};
        thread_guard g(_thread);

        thread _thread_1{[]() {
            // 用于Client2Server
            FeverRPC::Server rpc(threadmanager);
            rpc.bind("repeat", repeat);

            // 必须绑定login，用于验证身份
            rpc.bind("login", login);
            rpc.bind("echo", echo_thread);
            rpc.c2s();
        }};
        thread_guard gg(_thread_1);
    }
    return 0;
}
```

如果想要在其他文件中使用ThreadManager的推送功能，需要使用`extern`定义外部变量。