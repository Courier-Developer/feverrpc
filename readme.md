# FeverRPC

一个基于Socket长连接双向RPC框架，嵌入了登录认证功能。

至于为什么叫FeverRPC，是因为写的时候发烧了。

## Example

这个RPC可以让你写出**类似于**以下的代码

```C++
// Client.cpp
#include "feverrpc.hpp"

int main(){
    FeverRPC::Client rpc("127.0.0.1");

    int ans = rpc.call<int>("add", 1, 2, 3);
}
```

```C++
// Server
#include "feverrpc.hpp"

int add(int a, int b, int c){
    return a + b + c;
}

int main(){
    FeverRPC::Server rpc;

    rpc.bind("add", add);
    rpc.c2s(); 
}
```

## Features

- 使用TCP/Socket长连接
- 双向RPC
- 支持任意长度、类型参数绑定
- 基于Msgpack，可自定义序列化类型
- Socket支持任意大小传输功能（int)
- 支持多线程，有多线程调度模块
- 服务端线程可相互通信
- 嵌入登录功能

## Not Support

- void返回值

## Dependencies

你需要解决以下依赖

- msgpack
- C++17    `std::apply`
- Linux  未在其他平台上进行过测试

## Devlopment

请参阅相关文档 [dev-docs](./dev-documentation.md)

## Related Efforts

- 感谢 [button-rpc](https://github.com/button-chen/buttonrpc_cpp14) 给予了我最关键的知识点
- Blogs you can find on Google Search
