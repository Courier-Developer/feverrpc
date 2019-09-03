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


} // namespace FeverRPC