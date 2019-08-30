#pragma once
#include "msgpack.hpp"
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <pqxx/pqxx>
#include <queue>
#include <string>
#include <thread>
using std::cout;
using std::endl;
using std::map;
using std::queue;
using std::string;
using std::thread;


namespace push {

enum class Type { NOTIFY, SPETIAL };

class Push {
    Type type;
    string message;

  public:
    MSGPACK_DEFINE(type, message);
    Push(){};
    Push(Type tp, string msg) : type(tp), message(msg){};
    friend std::ostream &operator<<(std::ostream &os, Push &p) {
        std::string tp;
        switch (p.type) {
        case Type::NOTIFY:
            tp = "通知";
            break;

        default:
            tp = "特别通知";
            break;
        }
        os << "Push{" << tp << ',' << p.message << "}" << endl;
        return os;
    }
};

} // namespace push
MSGPACK_ADD_ENUM(push::Type);
class UserStatus {

  public:
    const thread::id thread_id;

    int count;

    queue<push::Push> pushes;

    UserStatus(thread::id tid) : thread_id(tid), count(0){};
    UserStatus() : count(0), thread_id(std::this_thread::get_id()){};
};

// 用于维护在线状态，用户对应的线程号, 心跳count，
// 消息队列，采用互斥锁解决竞争问题。
class ThreadManager {
    map<int, UserStatus> _status;
    std::mutex _mtx;
    static const int COUNT_LIMIT = 30;

  public:
    bool online(int uid) {
        std::lock_guard<std::mutex> guard(_mtx);
        if (_status.count(uid) <= 0) {
            return false;
        }
        if (_status[uid].count > COUNT_LIMIT) {
            // if timeout, unregister the user.
            _status.erase(uid);
            return false;
        }
        return true;
    }
    bool force_logout(int uid) {
        // 是否被强制下线，在确保曾经login情况下使用
        std::lock_guard<std::mutex> guard(_mtx);
        if (_status[uid].thread_id != std::this_thread::get_id()) {
            // 被新的登录挤下线
            return true;
        }
        return false;
    }
    bool reg(int uid, thread::id tid = std::this_thread::get_id()) {
        // register

        if (online(uid)&&force_logout(uid)) {
            print();
            cout << "[TM] 已检测到在线用户，干掉它" << endl;
            unreg(uid);
        }
        std::lock_guard<std::mutex> guard(_mtx);
        _status.insert(std::pair<int, UserStatus>({uid, {tid}}));
        // _status[uid] = UserStatus(tid);
        printf("[TM] register a new thread %lld from uid:%d.\n", tid, uid);
        print();
        return true;
    }
    bool unreg(int uid) {
        std::lock_guard<std::mutex> guard(_mtx);
        _status.erase(uid);
        printf("[TM] unregister uid %d from thread:%lld.\n", uid,
               std::this_thread::get_id());
        print();
        return true;
    }
    bool push(int uid, push::Push _p) {
        std::lock_guard<std::mutex> guard(_mtx);
        _status[uid].pushes.push(_p);
        return true;
    }
    bool have_push(int uid) {
        if (!online(uid))
            return false;
        std::lock_guard<std::mutex> guard(_mtx);
        if (_status[uid].pushes.empty())
            return false;

        return true;
    }
    push::Push get_push(int uid) {
        std::lock_guard<std::mutex> guard(_mtx);
        push::Push p = _status[uid].pushes.front();
        _status[uid].pushes.pop();
        return p;
    }
    friend std::ostream &operator<<(std::ostream &os,
                                    const ThreadManager &_tm) {
        cout << "{" << endl;
        for (auto elem : _tm._status) {
            cout << "    " << elem.first << " " << elem.second.thread_id
                 << "\n";
        }
        cout << "}\n";
        return os;
    }
    void print() {
        cout << "{" << endl;
        for (auto elem : _status) {
            cout << "    " << elem.first << " " << elem.second.thread_id
                 << "\n";
        }
        cout << "}\n";
    }
};


class thread_guard {
    std::thread &t;

  public:
    explicit thread_guard(std::thread &_t) : t(_t){};

    ~thread_guard() {
        puts("destructing thread");
        if (t.joinable()) {
            printf("[%lld]waiting for anthor thread to destruct %lld \n",
                   std::this_thread::get_id(), t.get_id());

            t.join();
            printf("[%lld]thread destructed\n", std::this_thread::get_id());
        }
    }

    thread_guard(const thread_guard &) = delete;
    thread_guard &operator=(const thread_guard &) = delete;
};