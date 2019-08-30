#pragma once
#include <string>
#include "msgpack.hpp"
using namespace std;

struct Login{
    string username;
    string password;  
    MSGPACK_DEFINE(username,password);
};

int login(Login){
    return 1;
}