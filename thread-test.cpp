#include <iostream>
#include <thread>
using namespace std;

class Task
{
    public:
        Task(){
            thread::id tid = this_thread::get_id();
            cout << "tid on create = "<< tid <<endl;
        }
        void operator()(int i){
            cout << i << endl;
            thread::id tid = this_thread::get_id();
            cout << "tid on call= "<< tid <<endl;
        }
        ~ Task(){
            thread::id tid = this_thread::get_id();

            cout << "tid on destroy= "<< tid <<endl;
        }
};

class thread_guard
{
    thread &t;
    public:
    explicit thread_guard(thread& _t):t(_t){};

    ~thread_guard()
    {
        if (t.joinable())
            t.detach();
    }

    thread_guard(const thread_guard&) = delete;
    thread_guard& operator=(const thread_guard&)= delete;
};

int main(){
    for (uint8_t i=0; i< 4;i++){
        Task task;
        thread t(task, i);
        thread_guard g(t);
    }


    return 0;
}
