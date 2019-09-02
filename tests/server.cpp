// Server side C/C++ program to demonstrate Socket programming
#include <feverrpc/feverrpc.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <feverrpc/threadmanager.hpp>
using namespace std;

ThreadManager threadmanager;

void task() {}

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
            FeverRPC::Server _rpc(threadmanager);
            _rpc.s2c();
        }};
        thread_guard g(_thread);

        thread _thread_1{[]() {
            FeverRPC::Server rpc(threadmanager);
            rpc.bind("repeat", repeat);
            rpc.bind("login", login);
            rpc.bind("echo", echo_thread);
            rpc.bind("read_file",read_file);
            rpc.c2s();
        }};
        thread_guard gg(_thread_1);
    }
    return 0;
}