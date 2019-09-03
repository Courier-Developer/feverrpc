#include <feverrpc/feverrpc-server.hpp>
namespace FeverRPC {
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