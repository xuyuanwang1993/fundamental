
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include <chrono>
#include <iostream>
#include <memory>
// set USE_RAW_PTR to  0 will cause memory segmentation
#define USE_RAW_PTR 1
namespace {
struct X {
    X() {
        str  = std::make_shared<std::string>();
        *str = std::string(1024, 'c');
        std::cout << "x construct" << std::endl;
    }
    ~X() {
        str.reset();
        std::cout << "x destruct" << std::endl;
    }
    std::shared_ptr<std::string> str;
};

struct TestInstance {
    ~TestInstance() {
        std::cout << "TestInstance destruct" << std::endl;
        delete x;
        x = nullptr;
    }
    std::string GetX() {
        return *(x->str);
    }
    void SetX() {
        *(x->str) = std::string(1024, 'b');
    }
    X* x = new X;
};
#if USE_RAW_PTR
static auto t1 = new TestInstance();
#else
static std::shared_ptr<TestInstance> t1 = std::make_shared<TestInstance>();
#endif
struct TestS : public Fundamental::Singleton<TestS> {
    // we should not declare any other signature for contruct Singleton instance
    // except the default signature version if you want to init some member values
    ~TestS();
    int i = 0;
};
struct TestNormal {
    ~TestNormal();
};
static TestNormal s_normal;
#if USE_RAW_PTR
static auto t2 = new TestInstance();
#else
static std::shared_ptr<TestInstance> t2 = std::make_shared<TestInstance>();
#endif
TestS::~TestS() {
    // we should not access another static instance
    //  in the destruct function to avoid access invalid memory segmentation
    std::cout << "TestS destruct" << std::endl;
    std::cout << "access t1 " << t1->GetX() << std::endl;
    std::cout << "access t2" << t2->GetX() << std::endl;
}
TestNormal::~TestNormal() {
    FINFOS << "TestNormal destruct";
    std::cout << "access t1 " << t1->GetX() << std::endl;
    std::cout << "access t2 " << t2->GetX() << std::endl;
}
} // namespace
int main(int argc, char** argv) {
    auto& a = t1;
    a->SetX();
    auto& c = t2;
    c->SetX();
    auto& b = TestS::Instance();
    F_UNUSED(b);
    auto& n = s_normal;
    F_UNUSED(n);
    return 0;
}
