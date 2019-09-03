#include <feverrpc/feverrpc.hpp>

namespace FeverRPC {

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

} // namespace FeverRPC