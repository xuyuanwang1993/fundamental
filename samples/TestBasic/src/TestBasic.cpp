
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include <chrono>
#include <iostream>
namespace
{
struct TestS : public Fundamental::Singleton<TestS>
{
    // we should not declare any other signature for contruct Singleton instance
    // except the default signature version if you want to init some member values
    ~TestS()
    {   
        //we should not access another static instance
        // in the destruct function to avoid access invalid memory segmentation
        //FINFO("destruct");
        std::cout<<"destruct"<<std::endl;
    }
    int i = 0;
};
} // namespace
int main(int argc, char** argv)
{
    auto& t = TestS::Instance();
    return 0;
}
