#include "feverrpc.cpp"
#include "msgpack.hpp"
#include <gtest/gtest.h>
#include <iostream>

class TestRPC : public testing::Test {
  protected:
    virtual void SetUp() override {}

    FeverRPC rpc;
};

int add(int a, int b, int c) { return a + b + c; }
// in below are some functions.
std::string adder(std::string a) {
    std::string ret = a + a;
    return ret;
}

// self defined class
class DIY {
  private:
    std::string name;
    int age;
    std::vector<float> scores;

  public:
    MSGPACK_DEFINE(name, age, scores);
    DIY(std::string name, int age, std::vector<float> scores)
        : name(name), age(age), scores(scores){};
    DIY(){};
    friend std::ostream &operator<<(std::ostream &os, DIY diy) {
        os << '{' << diy.name << ',' << diy.age << '[';
        for (auto s : diy.scores) {
            os << s << ',';
        }
        os << '}' << std::endl;
        return os;
    }
};

DIY aadd(std::string name, int age, std::vector<float> scores) {
    return DIY(name, age, scores);
}

TEST(feverrpc, test_of_test) { EXPECT_EQ(1, 1); };

TEST_F(TestRPC, simple_sum) {

    rpc.bind("add", add);
    rpc.bind("adder", adder);
    rpc.bind("diy", aadd);
    rpc.call<int>("add", 1, 2, 4);
    rpc.call<int>("add", 1, 23, 1, 1);
    std::string name = "WangIdon";
    int age = 1000;
    std::vector<float> fs = {.123, 3.2, 323.2};
    rpc.call<DIY>("diy", name, age, fs);
    // ASSERT_EQ(1, 1);
    // ASSERT_EQ(rpc.call<int>("add", 3, 9, 0), 12);
};

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// in below are some functions.
int addee(std::vector<int> va, std::vector<int> vb, int c) {
    int ret = 0;
    for (auto v : va) {
        ret += v * v * c;
    }
    for (auto v : vb) {
        ret += v * v / c;
    }
    return ret;
}

std::string addd(std::vector<std::string> vs) {
    std::string ret = "";
    for (auto v : vs) {
        ret += "|" + v;
    }
    return ret;
}

// int main() {
//     using std::cout;
//     using std::endl;
//     FeverRPC rpc;
//     rpc.as_client();
//     // register functions.
//     rpc.bind("add", add);
//     rpc.bind("test", adder);
//     std::string a = "hello";
//     std::string b = ", world! ";

//     // call functions.
//     rpc.call<void>("add", 1, 2);
//     cout << rpc.call<std::string>("test", a) << endl;
//     std::vector<int> va = {1, 2, 3, 4, 5};
//     std::vector<int> vb = {2, 4, 6, 8, 10};
//     rpc.bind("addee", addee);
//     cout << rpc.call<int>("addee", va, vb, 2) << endl;
//     rpc.bind("addd", addd);
//     std::vector<std::string> vue = {"12312", "nonono", "balabong"};
//     cout << rpc.call<std::string>("add", vue) << endl;
//     std::string name = "WangIdon";
//     int age = 1000;
//     std::vector<float> fs = {.123, 3.2, 323.2};
//     rpc.bind("aadd", aadd);
//     cout << rpc.call<DIY>("aadd", name, age, fs) << endl;
// }
