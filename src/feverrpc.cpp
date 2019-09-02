#include <cstdio>
#include <feverrpc/feverrpc.hpp>
#include <netinet/in.h>
#include <sys/socket.h>

namespace FeverRPC {

int send_data(const int &socket_handler, const char *data_send_buffer,
              int data_send_size) {
    int err = 0;
    err =
        send(socket_handler, (void *)&data_send_size, sizeof(unsigned int), 0);
    if (err < 0) {
        puts("error < 0 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    int sent_size = 0;

    while (sent_size < data_send_size) {
        int _size = sent_size + _CHUNK_SIZE < data_send_size
                        ? _CHUNK_SIZE
                        : (data_send_size - sent_size);

        err = send(socket_handler, (void *)&data_send_buffer[sent_size], _size,
                   0);

        if (err < 0) {
            puts("error < 0 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }
        sent_size += _size;
    }
    return err;
}

int recv_data(const int &socket_handler, msgpack::sbuffer &data_recv_buffer) {
    int recv_len = 0;
    int err = 0;
    char _buffer[_CHUNK_SIZE + 10] = {};
    err = read(socket_handler, (void *)&recv_len, sizeof(unsigned int));
    if (err < 0) {
        puts("error < 0 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    int recv_size = 0;
    while (recv_size < recv_len) {
        int _size = recv_size + _CHUNK_SIZE < recv_len ? _CHUNK_SIZE
                                                       : (recv_len - recv_size);
        // 使用 _buffer 缓冲数据
        err = read(socket_handler, _buffer, _CHUNK_SIZE);
        // 直接写入 msgpack::sbuffer
        if (err < 0) {
            puts("error < 0 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }
        data_recv_buffer.write(_buffer, _size);

        recv_size += _size;
    }

    return err;
}

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
        SocketException e;
        throw e;
    }
    return err;
}

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

Serializer FeverRPC::call_(std::string name, msgpack::object args_obj) {
    printf("[%lld]in [call_], retrieving function.\n",
           std::this_thread::get_id());
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

int FeverRPC::send_and_recv(const int &socket_handler,
                            const char *data_send_buffer, int data_send_size,
                            msgpack::sbuffer &data_recv_buffer) {
    int err = 0;
    err += send_data(socket_handler, data_send_buffer, data_send_size);
    err += recv_data(socket_handler, data_recv_buffer);
    return err;
}
Serializer FeverRPC::recv_call_and_send(const int &socket_handler) {
    // 将会存储 name 和 args
    msgpack::sbuffer msg_buffer;

    // ! recv_data from socket.
    recv_data(socket_handler, msg_buffer);
    // https://github.com/msgpack/msgpack-c/blob/master/QUICKSTART-CPP.md#streaming-feature
    // deserializes these objects using msgpack::unpacker.
    if (msg_buffer.size() == 0) {
        SocketException e;
        throw e;
    }
    print_sbuffer(msg_buffer);
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
    Serializer ans = call_chooser(func_name, args_obj);
    // ! 3. buffer -> msgpack::sbuffer.
    Serializer ret;
    ret.buffer.write(ans.buffer.data(), ans.buffer.size());
    // ! 4. send data
    send_data(socket_handler, ans.buffer.data(), ans.buffer.size());
    printf("[recvcallandsend] buffer.data() %d", ret.buffer.size());
    return ret;
}

Serializer FeverRPC::call_chooser(std::string &func_name,
                                  msgpack::object &args_obj) {
    return call_(func_name, args_obj);
}

void FeverRPC::print_sbuffer(msgpack::sbuffer &buffer) {
    puts("[print_sbuffer]start");
    std::cout << buffer.size() << std::endl;
    // copy buffer
    msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());

    // deserialized object is valid during the msgpack::object_handle instance
    // is alive.
    msgpack::object deserialized = oh.get();

    // msgpack::object supports ostream.
    std::cout << '[' << std::this_thread::get_id() << ']' << deserialized
              << std::endl;
    puts("[print_sbuffer]end");
}

// FeverRPC::Client
Client::Client(const char *host) {
    // client
    _rpc_role = rpc_role::RPC_CLINT;
    _rpc_ret_code = rpc_ret_code::RPC_RET_SUCCESS;
    _HOST = host;
    _c2s_socket_handler = -1;
    _s2c_socket_handler = -1;
}
Client::~Client() {
    printf("_c2s=%d,_s2c=%d\n", _c2s_socket_handler, _s2c_socket_handler);
    if (_c2s_socket_handler > 0) {
        printf("[%lld]close _c2s\n", std::this_thread::get_id());
        close(_c2s_socket_handler);
    }

    if (_s2c_socket_handler > 0) {
        printf("[%lld]close _s2c\n", std::this_thread::get_id());
        close(_s2c_socket_handler);
    }
}
void Client::s2c() {
    printf("[%lld]in [s2c], waiting...\n", std::this_thread::get_id());
    // try to build a long link socket to server
    connect_socket(_HOST, _S2C_PORT, _s2c_socket_handler);
    printf("dd%d", _s2c_socket_handler);
    while (1) {
        // equal to recv_call_and_send

        listen(_s2c_socket_handler);
    }
    printf("enddd dd%d", _s2c_socket_handler);

    printf("[%lld]leave [s2c], exits\n", std::this_thread::get_id());
}

void Client::listen(const int &_s2c_socket_handler) {
    recv_call_and_send(_s2c_socket_handler);
}
// FeverRPC::Server

Server::Server(ThreadManager &threadManager) : threadManager(threadManager) {
    // server
    printf("[%lld][initialize server]\n", std::this_thread::get_id());
    _rpc_role = rpc_role::RPC_SERVER;
    _rpc_ret_code = rpc_ret_code::RPC_RET_SUCCESS;
    _HOST = "";
}
Server::~Server() {
    printf("[%lld]destruct Server.\n", std::this_thread::get_id());
}

void Server::s2c() {
    // 一个监听线程，用于服务端向客户端推送消息
    // 首先，监听socket，当有连接时，新建线程
    // 服务端会立刻调用login函数，获得用户名和密码。
    // 然后向数据库进行认证，如果失败，则断开链接。
    // 如果成功，证明该用户登录。存储用户id，并向线程调度模块注册线程，证明该用户上线。
    printf("[%lld]in [s2c], waiting...\n", std::this_thread::get_id());
    // try to initialize a socket in another thread.
    int server_fd, new_socket_handler;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // initialize socket
    initialize_socket(_S2C_PORT, server_fd, address);
    // block and wait for connect
    while (1) {
        printf("[%lld][s2c] waiting for new socket to connect.\n",
               std::this_thread::get_id());
        if ((new_socket_handler = accept(server_fd, (struct sockaddr *)&address,
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
            threadManager.reg(uid, new_socket_handler);
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
                    printf("[%lld]I want to push this\n",
                           std::this_thread::get_id());
                    note<int>(new_socket_handler, "push",
                              push::Push{push::Type::NOTIFY,
                                         std::string("你已被强制下线。")});
                    close(new_socket_handler);
                    break;
                } else if (threadManager.have_push(uid) == false) {
                    printf("[%lld]没有通知\n", std::this_thread::get_id());
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    std::this_thread::yield();
                } else {
                    push::Push p = threadManager.get_push(uid);
                    std::cout << "有通知" << p << endl;

                    note<int>(new_socket_handler, "push", p);
                }
            }

            printf("[%lld][s2c] leave thread\n", std::this_thread::get_id());
        }};
        _thread.detach();
    };
    printf("[%lld][Server::s2c]this thread ended!\n",
           std::this_thread::get_id());
}

void Server::c2s() {

    printf("[%lld]in [c2s], waiting...\n", std::this_thread::get_id());
    // run socket server
    // ! 1. initialize socket
    int server_fd, new_socket_handler;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    initialize_socket(_C2S_PORT, server_fd, address);

    // ! 2. block and waiting for data.
    while (1) {
        printf("[%lld][c2s] waiting for new socket to connect.\n",
               std::this_thread::get_id());
        if ((new_socket_handler = accept(server_fd, (struct sockaddr *)&address,
                                         (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        std::thread _thread{[new_socket_handler, this]() {
            int uid = -1;
            printf("[new thread][c2s] %lld\n", std::this_thread::get_id());
            puts("waiting for login/register");
            // 这里同时绑定两个函数 login / register, 可以同时处理登录 / 注册
            // 两个事件
            Serializer _ans = recv_call_and_send(new_socket_handler);
            uid = unpack_ret_val<int>(_ans.buffer);
            if (uid < 0) {
                // 认证失败,退出
                close(new_socket_handler);
                return;
            }
            // 这以后，被调用函数可以通过TM获取uid
            threadManager.reg_thread(uid);
            std::cout << "[c2s]认证成功" << std::endl;
            while (1) {
                try {
                    recv_call_and_send(new_socket_handler);
                    printf("[%lld]recv_call_and_send(new_socket_handler);\n",
                           std::this_thread::get_id());
                } catch (const std::exception &e) {
                    // 这里会在客户端退出时捕获异常，并进行逻辑处理
                    // 基本的思想是，利用threadManager
                    // 释放另一个线程的资源（socket handler 等）
                    // 然后释放自己的资源
                    if (threadManager.online(uid)) {
                        puts("release thread");
                        threadManager.unreg(uid);
                    }
                    puts(e.what());

                    break;
                }
            }
            printf("[%lld][c2s]this thread ended!\n",
                   std::this_thread::get_id());
        }};
        _thread.detach();
    }

    printf("[%lld][Server::c2s]this thread ended!\n",
           std::this_thread::get_id());
    return;
};

} // namespace FeverRPC