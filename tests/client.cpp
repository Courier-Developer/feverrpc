// Client side C/C++ program to demonstrate Socket programming
#include <feverrpc/feverrpc.hpp>
#include <feverrpc/utils.hpp>
#include <string>
#include <thread>
#include <feverrpc/utils.hpp>
using namespace std;

int sub(int a, int b) { return a - b; }

int deal_push(push::Push p) {
    cout << p << endl;
    return 2;
}

Login ret_login_info() {
    return Login{username : "example", password : "password"};
}

int main(int argc, char const *argv[]) {
    FeverRPC::Client rpc("127.0.0.1");

    thread _thread{[]() {
        FeverRPC::Client _rpc("127.0.0.1");
        _rpc.bind("sub", sub);
        _rpc.bind("login", ret_login_info);
        _rpc.bind("push", deal_push);
        _rpc.s2c();
    }};
    thread_guard g(_thread);
    rpc.call<int>("login", Login{username : "uname", password : "pwod"});
    vector<char> data = rpc.call<std::vector<char>>("read_file", "tests/client.cpp");
    save_file("bin/client.txt",data);
    this_thread::sleep_for(chrono::seconds(2000));
    string ans = rpc.call<string>("repeat", "Yes! ", 5);
    cout << ans << endl; // Yes! Yes! Yes! Yes! Yes!
    int ans2 = rpc.call<int>("echo");
    cout <<"login result: "<< ans2 << endl; // Yes! Yes! Yes! Yes! Yes!

    return 0;
}
