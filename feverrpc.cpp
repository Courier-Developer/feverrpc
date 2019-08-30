#include "lock.cpp"
#include "msgpack.hpp"
#include "utils.cpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
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


namespace FeverRPC {

// Socket一次通信中最大数据发送大小
const int _CHUNK_SIZE = 1024;

// 客户端到服务器通信端口
const int _C2S_PORT = 8012;
// 服务端到客户端通信端口
const int _S2C_PORT = 8021;

// Socket通讯异常
class SocketException : public std::exception {
    virtual const char *what() const throw() { return "Socket　Error"; }
};

// 函数调用异常，说明你没有注册该函数
class FunctionCallException : public std::exception {

    virtual const char *what() const throw() {
        return "function call error. no such function.";
    }
};


// 使用Socket发送数据
int send_data(const int &socket_handler, const char *data_send_buffer,
              int data_send_size) {
    int err = 0;
    printf("\n[SEND_START] %s\n", data_send_buffer);
    // printf("[HANDLER] %d\n", socket_handler);
    // printf("errno: %d", errno);
    // try {
    err = send(socket_handler, (void *)&data_send_size, sizeof(int), 0);
    // printf("[SIZE] %d\n", data_send_size);
    // printf("send err value(should be 4(int)): %d\n", err);
    // printf("errno: %d", errno);

    int sent_size = 0;

    while (sent_size < data_send_size) {
        int _size = sent_size + _CHUNK_SIZE < data_send_size
                        ? _CHUNK_SIZE
                        : (data_send_size - sent_size);
        // printf("--[CHUNCK] before send data\n socket_handler: %d\n",
        //    socket_handler);

        err = send(socket_handler, (void *)&data_send_buffer[sent_size], _size,
                   0);
        // printf("--[CHUNCK] err %d\n", err);
        // printf("--[CHUNCK] %d\n", _size);

        if (err < 0) {
            // deal with error.
        }
        sent_size += _size;
    }
    // } catch (const std::exception &e) {
    //     puts(e.what());
    //     puts("oooops! send_data error.");
    // }
    puts("[SEND_DONE]");
    return err;
}

// 接收数据
int recv_data(const int &socket_handler, msgpack::sbuffer &data_recv_buffer) {
    int recv_len = 0;
    int err = 0;
    char _buffer[_CHUNK_SIZE + 10] = {};
    puts("\n[RECV_START]");
    // printf("[HANDLER] %d\n", socket_handler);
    err = read(socket_handler, (void *)&recv_len, sizeof(int));
    // printf("recv err value: %d\n", err);
    // printf("[SIZE] %d\n", recv_len);

    int recv_size = 0;
    while (recv_size < recv_len) {
        int _size = recv_size + _CHUNK_SIZE < recv_len ? _CHUNK_SIZE
                                                       : (recv_len - recv_size);
        // 使用 _buffer 缓冲数据
        err = read(socket_handler, _buffer, _CHUNK_SIZE);
        // 直接写入 msgpack::sbuffer
        // printf("recv err value: %d\n", err);

        data_recv_buffer.write(_buffer, _size);
        // printf("--[CHUNCK] %d\n", _size);

        if (err < 0) {
            // deal with error.
        }
        recv_size += _size;
    }
    printf("[RECV_DONE] %s\n", data_recv_buffer.data());

    return err;
}

// 连接到一个Socket，用户客户端。
int connect_socket(const char *__host, const int __port,
                   int &new_socket_handler) {
    // return err;
    // TODO: better error!
    int err = 0;

    sockaddr_in serv_addr;
    if ((new_socket_handler = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        exit(-1);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(__port);
    if (inet_pton(AF_INET, __host, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        exit(-1);
    };
    if (connect(new_socket_handler, (sockaddr *)&serv_addr, sizeof(serv_addr)) <
        0) {
        printf("\nConnection Failed \n");
        exit(-1);
    }
    return err;
}

// 初始化Socket，监听端口。用于服务端。
int initialize_socket(const int __port, int &server_fd, sockaddr_in &address) {
    int opt = 1;
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt\n");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(__port);

    // Forcefully attaching socket to the port 8012
    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
    if (::listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return 0;
}

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
    void callproxy(Func f, Serializer *pr, msgpack::object args_obj) {
        callproxy_(f, pr, args_obj);
    };
    template <typename Ret, typename... Args>
    void callproxy_(Ret (*func)(Args...), Serializer *pr,
                    msgpack::object args_obj) {

        callproxy_(std::function<Ret(Args...)>(func), pr, args_obj);
    }
    template <typename Ret, typename... Args>
    void callproxy_(std::function<Ret(Args... ps)> func, Serializer *pr,
                    msgpack::object args_obj) {
        std::tuple<Args...> args;
        args_obj.convert(args);

        Ret ret = call_helper<Ret>(func, args);

        msgpack::pack(pr->buffer, ret);
        return;
    }
    template <typename Ret, typename Func, typename... Args>
    Ret call_helper(Func f, std::tuple<Args...> args) {
        return std::apply(f, args);
    }
    Serializer call_(std::string name, msgpack::object args_obj) {
        puts("in [call_], retrieving function.");
        Serializer ds;
        if (funcs_map.count(name) == 0) {
            printf("function name error. name:%s\n", name.c_str());
            FunctionCallException e;
            throw e;
        }
        // do some hijack;
        // 可以在这里检查比如登录功能。
        auto fun = funcs_map[name];
        fun(&ds, args_obj);
        return ds;
    }

    // more helper functions.
    int send_and_recv(const int &socket_handler, const char *data_send_buffer,
                      int data_send_size, msgpack::sbuffer &data_recv_buffer) {
        int err = 0;
        err += send_data(socket_handler, data_send_buffer, data_send_size);
        err += recv_data(socket_handler, data_recv_buffer);
        return err;
    }

    template <typename Ret> Ret unpack_ret_val(msgpack::sbuffer &buffer) {

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

    Serializer recv_call_and_send(const int &socket_handler) {
        // 将会存储 name 和 args
        msgpack::sbuffer msg_buffer;

        // ! recv_data from socket.
        recv_data(socket_handler, msg_buffer);
        // https://github.com/msgpack/msgpack-c/blob/master/QUICKSTART-CPP.md#streaming-feature
        // deserializes these objects using msgpack::unpacker.

        msgpack::unpacker pac;
        // feeds the buffer.
        pac.reserve_buffer(msg_buffer.size());
        memcpy(pac.buffer(), msg_buffer.data(), msg_buffer.size());
        pac.buffer_consumed(msg_buffer.size());
        // now starts streaming deserialization.
        msgpack::object_handle oh;
        pac.next(oh);
        msgpack::object name_obj = oh.get();
        pac.next(oh);
        msgpack::object args_obj = oh.get();
        Serializer ds;
        // get `func_name`.
        std::string func_name;
        name_obj.convert(func_name);

        // ! execute funciton with args.
        Serializer ans = call_(func_name, args_obj);
        // ! 3. buffer -> msgpack::sbuffer.
        Serializer ret;
        ret.buffer.write(ans.buffer.data(), ans.buffer.size());
        // ! 4. send data
        send_data(socket_handler, ans.buffer.data(), ans.buffer.size());
        printf("[recvcallandsend] buffer.data() %d", ret.buffer.size());
        return ret;
    }
};

template <typename Func> void FeverRPC::bind(std::string name, Func func) {
    funcs_map[name] = std::bind(&FeverRPC::callproxy<Func>, this, func,
                                std::placeholders::_1, std::placeholders::_2);
}

class FeverRPC_client : public FeverRPC {};

class Client : public FeverRPC {
  private:
    int _c2s_socket_handler;
    int _s2c_socket_handler;

  public:
    Client(const char *host) {
        // client
        _rpc_role = rpc_role::RPC_CLINT;
        _rpc_ret_code = rpc_ret_code::RPC_RET_SUCCESS;
        _HOST = host;
        _c2s_socket_handler = -1;
        _s2c_socket_handler = -1;
    }
    ~Client() {
        printf("_c2s=%d,_s2c=%d\n", _c2s_socket_handler, _s2c_socket_handler);
        if (_c2s_socket_handler > 0) {
            puts("close _c2s");
            close(_c2s_socket_handler);
        }

        if (_s2c_socket_handler > 0) {
            puts("close _s2c");
            close(_s2c_socket_handler);
        }
    }
    // client
    void s2c() {
        puts("in [s2c], waiting...");
        // try to build a long link socket to server
        connect_socket(_HOST, _S2C_PORT, _s2c_socket_handler);
        printf("dd%d", _s2c_socket_handler);
        while (1) {
            listen(_s2c_socket_handler);
        }
        printf("enddd dd%d", _s2c_socket_handler);

        puts("leave [s2c], exits");
    }
    template <typename Ret, typename... Args>
    Ret call(std::string name, Args... args) {
        // auto args_tuple = std::make_tuple(args...);

        // Serializer ds;
        // ds << name;

        msgpack::sbuffer buffer;

        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(name);
        pk.pack(std::make_tuple(args...));

        return socket_call<Ret>(buffer);
    }
    // client
    template <typename Ret> Ret socket_call(msgpack::sbuffer &buffer) {
        if (_c2s_socket_handler < 0) {
            connect_socket(_HOST, _C2S_PORT, _c2s_socket_handler);
        }
        // socket_call_(socket_handler, buffer.data(), buffer.size(), buff);
        msgpack::sbuffer recv_buff;
        send_and_recv(_c2s_socket_handler, buffer.data(), buffer.size(),
                      recv_buff);
        // deserializes these objects using msgpack::unpacker.
        return unpack_ret_val<Ret>(recv_buff);
    }
    void listen(const int &_s2c_socket_handler) {
        recv_call_and_send(_s2c_socket_handler);
    }
};

class Server : public FeverRPC {
    ThreadManager &threadManager;

  public:
    std::thread::id _s2c_thread_id;
    Server(ThreadManager &threadManager) : threadManager(threadManager) {
        // server
        puts("[initialize server]");
        _rpc_role = rpc_role::RPC_SERVER;
        _rpc_ret_code = rpc_ret_code::RPC_RET_SUCCESS;
        _HOST = "";
    }
    ~Server() { puts("destruct Server."); }
    void s2c() {
        // 一个监听线程，用于服务端向客户端推送消息
        // 首先，监听socket，当有连接时，新建线程
        // 服务端会立刻调用login函数，获得用户名和密码。
        // 然后向数据库进行认证，如果失败，则断开链接。
        // 如果成功，证明该用户登录。存储用户id，并向线程调度模块注册线程，证明该用户上线。
        puts("in [s2c], waiting...");
        // try to initialize a socket in another thread.
        int server_fd, new_socket_handler;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        // initialize socket
        initialize_socket(_S2C_PORT, server_fd, address);
        // block and wait for connect
        while (1) {
            puts("[s2c] waiting for new socket to connect.");
            if ((new_socket_handler =
                     accept(server_fd, (struct sockaddr *)&address,
                            (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            // 新建线程
            std::thread _thread{[new_socket_handler, this]() {
                int uid = -1;
                printf("[new thread][s2c] %lld\n", std::this_thread::get_id());
                // 首先调用login函数
                Login _login = note<Login>(new_socket_handler, "login");
                printf("username:%s,password:%s\n", _login.username.c_str(),
                       _login.password.c_str());

                uid = login(_login);
                if (uid < 0) {
                    // 认证失败，将断开连接
                    close(new_socket_handler);
                    return;
                }
                // 认证成功，向ThreadManager注册自己
                threadManager.reg(uid);
                std::cout << "[s2c]认证成功，并注册" << std::endl;

                while (1) {
                    bool online = threadManager.online(uid);
                    if (!online)
                        break;
                    bool logout = threadManager.force_logout(uid);
                    if (online && logout) {
                        // 被强制登出了
                        // Do somethings to notify client
                        // that he was logoff.
                        // 客户端最好迅速断开另一个socket连接
                        note<int>(new_socket_handler, "push",
                                  push::Push{push::Type::NOTIFY,
                                             std::string("你已被强制下线。")});
                        break;
                    }
                    else if (threadManager.have_push(uid) == false) {
                        puts("没有通知");
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        std::this_thread::yield();
                    } else {
                        push::Push p = threadManager.get_push(uid);
                        std::cout << "有通知" << p << endl;

                        note<int>(new_socket_handler, "push", p);
                    }
                }

                puts("[s2c] leave thread");
            }};
            _thread.detach();
        };
    }

    void c2s() {

        puts("in [c2s], waiting...");
        // run socket server
        // ! 1. initialize socket
        int server_fd, new_socket_handler;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        initialize_socket(_C2S_PORT, server_fd, address);

        // ! 2. block and waiting for data.
        while (1) {
            puts("[c2s] waiting for new socket to connect.");
            if ((new_socket_handler =
                     accept(server_fd, (struct sockaddr *)&address,
                            (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            std::thread _thread{[new_socket_handler, this]() {
                int uid = -1;
                printf("[new thread][c2s] %lld\n", std::this_thread::get_id());
                Serializer _ans = recv_call_and_send(new_socket_handler);
                printf("[c2s]_ans.size() %d\n", _ans.buffer.size());
                uid = unpack_ret_val<int>(_ans.buffer);
                if (uid < 0) {
                    // 认证失败,退出
                    close(new_socket_handler);
                    return;
                }
                std::cout << "[c2s]认证成功" << std::endl;
                while (1) {
                    try {
                        recv_call_and_send(new_socket_handler);
                        puts("recv_call_and_send(new_socket_handler);");
                    } catch (const std::exception &e) {
                        threadManager.unreg(uid);
                        puts(e.what());
                        puts("[c2s]this thread ended!");

                        break;
                    }
                }
            }};
            thread_guard g(_thread);
        }
        return;
    };
    template <typename Ret, typename... Args>
    Ret note(const int socket_handler, std::string name, Args... args) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(name);
        pk.pack(std::make_tuple(args...));
        return note_socket_call<Ret>(socket_handler, buffer);
    }

  private:
    template <typename Ret>
    Ret note_socket_call(const int socket_handler, msgpack::sbuffer &buffer) {
        send_data(socket_handler, buffer.data(), buffer.size());
        msgpack::sbuffer recv_buff;
        recv_data(socket_handler, recv_buff);
        return unpack_ret_val<Ret>(recv_buff);
    }
};
}; // namespace FeverRPC
