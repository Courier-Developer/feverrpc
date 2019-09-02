/*!
 * \file feverrpc.cpp
 * \brief 一个基于Socket长连接双向RPC框架，嵌入了登录认证功能。
 *
 * https://github.com/Courier-Developer/feverrpc
 * 使用TCP/Socket长连接
 * 双向RPC
 * 支持任意长度、类型参数绑定
 * 基于Msgpack，可自定义序列化类型
 * Socket支持任意大小传输功能（int)
 * 支持多线程，有多线程调度模块
 * 服务端线程可相互通信
 * 嵌入登录功能
 *
 * \author 冯开宇
 * \version 0.1
 * \date 2019-9-1
 */
#ifndef FEVERRPC_FEVERRPC_H_
#define FEVERRPC_FEVERRPC_H_

#include "msgpack.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <feverrpc/threadmanager.hpp>
#include <feverrpc/utils.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// TODO MORE DOCS

/// \brief RPC框架的命名空间
///
/// 包含RPC本身，socket通讯相关函数以及自定义异常
namespace FeverRPC {

/// \brief Socket一次通信中最大数据发送大小
const int _CHUNK_SIZE = 1024;

/// \brief 客户端到服务器通信端口
const int _C2S_PORT = 8012;

/// \brief 服务端到客户端通信端口
const int _S2C_PORT = 8021;

/// \brief Socket通讯异常
class SocketException : public std::exception {
    virtual const char *what() const throw() { return "Connection Failed / Socket Error"; }
};

/// \brief 函数调用异常，说明你没有注册该函数
class FunctionCallException : public std::exception {

    virtual const char *what() const throw() {
        return "function call error. no such function.";
    }
};

/// \brief 使用Socket发送数据
///
/// 支持unsigned int大小内的任意大小数据传输，不支持断点续传
/// \param socket_handler socket句柄
/// \param data_send_buffer the pointer that point to your data to be sent
/// \param data_send_size size of data
/// \return if there is error. 0 is ok.
int send_data(const int &socket_handler, const char *data_send_buffer,
              int data_send_size);

/// \brief 接收数据
/// \param socket_handler docket handler
/// \param data_recv_buffer msgpack::sbuffer used for recive data
/// \return if there is an error.
int recv_data(const int &socket_handler, msgpack::sbuffer &data_recv_buffer);

// 连接到一个Socket，用户客户端。
int connect_socket(const char *__host, const int __port,
                   int &new_socket_handler);

// 初始化Socket，监听端口。用于服务端。
int initialize_socket(const int __port, int &server_fd, sockaddr_in &address);

// 包装序列化数据
class Serializer {
  public:
    msgpack::sbuffer buffer;
};

// 封装RPC的基本方法
class FeverRPC {

  protected:
    //  强枚举类型
    enum class rpc_role { RPC_CLINT, RPC_SERVER };
    enum class rpc_ret_code : unsigned int {
        RPC_RET_SUCCESS = 0,
        RPC_RET_FUNCTION_NOT_BIND,
        RPC_RET_RECV_TIMEOUT,
        RPC_RET_UNRETURNED,
    };

    rpc_role _rpc_role;
    rpc_ret_code _rpc_ret_code;
    const char *_HOST;
    std::map<std::string, std::function<void(Serializer *, msgpack::object)>>
        funcs_map;

  public:
    // server
    template <typename Func> void bind(std::string name, Func func);

  protected:
    // both
    template <typename Func>
    void callproxy(Func f, Serializer *pr, msgpack::object args_obj);
    template <typename Ret, typename... Args>
    void callproxy_(Ret (*func)(Args...), Serializer *pr,
                    msgpack::object args_obj);
    template <typename Ret, typename... Args>
    void callproxy_(std::function<Ret(Args... ps)> func, Serializer *pr,
                    msgpack::object args_obj);
    template <typename Ret, typename Func, typename... Args>
    Ret call_helper(Func f, std::tuple<Args...> args);

    Serializer call_(std::string name, msgpack::object args_obj);

    Serializer call_chooser(std::string &name, msgpack::object &args_obj);
    // more helper functions.
    int send_and_recv(const int &socket_handler, const char *data_send_buffer,
                      int data_send_size, msgpack::sbuffer &data_recv_buffer);

    template <typename Ret> Ret unpack_ret_val(msgpack::sbuffer &buffer);

    Serializer recv_call_and_send(const int &socket_handler);

    void print_sbuffer(msgpack::sbuffer &buffer);
};

class Client : public FeverRPC {
  private:
    int _c2s_socket_handler;
    int _s2c_socket_handler;

  public:
    Client(const char *host);
    ~Client();
    // client
    void s2c();
    template <typename Ret, typename... Args>
    Ret call(std::string name, Args... args);
    // client
    template <typename Ret> Ret socket_call(msgpack::sbuffer &buffer);
    void listen(const int &_s2c_socket_handler);
};

class Server : public FeverRPC {
    ThreadManager &threadManager;

  public:
    Server(ThreadManager &threadManager);
    ~Server();
    void s2c();
    void c2s();

  private:
    template <typename Ret, typename... Args>
    Ret note(const int socket_handler, std::string name, Args... args);
    template <typename Ret>
    Ret note_socket_call(const int socket_handler, msgpack::sbuffer &buffer);
};

/////////////////////////////////////////////////////////////
//
/// SOME DEFINITIONS CANNOT TO BE DEFINED IN ANNOTHER FILE
//
/////////////////////////////////////////////////////////////
template <typename Func> void FeverRPC::bind(std::string name, Func func) {
    funcs_map[name] = std::bind(&FeverRPC::callproxy<Func>, this, func,
                                std::placeholders::_1, std::placeholders::_2);
}

template <typename Func>
inline void FeverRPC::callproxy(Func f, Serializer *pr,
                                msgpack::object args_obj) {
    callproxy_(f, pr, args_obj);
};
template <typename Ret, typename... Args>
inline void FeverRPC::callproxy_(Ret (*func)(Args...), Serializer *pr,
                                 msgpack::object args_obj) {

    callproxy_(std::function<Ret(Args...)>(func), pr, args_obj);
}
template <typename Ret, typename... Args>
void FeverRPC::callproxy_(std::function<Ret(Args... ps)> func, Serializer *pr,
                          msgpack::object args_obj) {
    std::tuple<Args...> args;
    args_obj.convert(args);

    Ret ret = call_helper<Ret>(func, args);

    msgpack::pack(pr->buffer, ret);
    return;
}
template <typename Ret, typename Func, typename... Args>
Ret FeverRPC::call_helper(Func f, std::tuple<Args...> args) {
    return std::apply(f, args);
}

template <typename Ret> Ret FeverRPC::unpack_ret_val(msgpack::sbuffer &buffer) {

    msgpack::unpacker pac;
    // feeds the buffer.
    pac.reserve_buffer(buffer.size());
    memcpy(pac.buffer(), buffer.data(), buffer.size());
    pac.buffer_consumed(buffer.size());

    // now starts streaming deserialization.
    msgpack::object_handle oh;
    pac.next(oh);
    msgpack::object obj = oh.get();
    Ret ret;
    obj.convert(ret);
    return ret;
}

// FeverRPC::Client

template <typename Ret, typename... Args>
Ret Client::call(std::string name, Args... args) {
    // auto args_tuple = std::make_tuple(args...);

    // Serializer ds;
    // ds << name;

    msgpack::sbuffer buffer;

    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(name);
    pk.pack(std::make_tuple(args...));

    return socket_call<Ret>(buffer);
}

template <typename Ret> Ret Client::socket_call(msgpack::sbuffer &buffer) {
    if (_c2s_socket_handler < 0) {
        connect_socket(_HOST, _C2S_PORT, _c2s_socket_handler);
    }
    // socket_call_(socket_handler, buffer.data(), buffer.size(), buff);
    msgpack::sbuffer recv_buff;
    printf("[send_and_recv]");
    print_sbuffer(buffer);
    send_and_recv(_c2s_socket_handler, buffer.data(), buffer.size(), recv_buff);
    // deserializes these objects using msgpack::unpacker.
    return unpack_ret_val<Ret>(recv_buff);
}

// FeverRPC::Server

template <typename Ret, typename... Args>
Ret Server::note(const int socket_handler, std::string name, Args... args) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(name);
    pk.pack(std::make_tuple(args...));
    return note_socket_call<Ret>(socket_handler, buffer);
}

template <typename Ret>
Ret Server::note_socket_call(const int socket_handler,
                             msgpack::sbuffer &buffer) {
    send_data(socket_handler, buffer.data(), buffer.size());
    msgpack::sbuffer recv_buff;
    recv_data(socket_handler, recv_buff);
    return unpack_ret_val<Ret>(recv_buff);
}

}; // namespace FeverRPC

#endif // FEVERRPC_FEVERRPC_H_