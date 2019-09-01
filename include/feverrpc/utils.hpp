#ifndef FEVERRPC_UTILS_H_
#define FEVERRPC_UTILS_H_

#include "msgpack.hpp"
#include <string>
using namespace std;

struct Login {
    string username;
    string password;
    MSGPACK_DEFINE(username, password);
};

int login(Login);

#endif // FEVERRPC_UTILS_H_