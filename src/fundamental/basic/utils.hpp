#ifndef _HEAD_BASIC_UTILS_
#define _HEAD_BASIC_UTILS_
#include <chrono>
#include <functional>
namespace Fundamental {

struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

using BasicTaskFunctionT=std::function<void()>;
struct ScopeGuard final : NonCopyable
{
    ScopeGuard(const BasicTaskFunctionT & _f,const BasicTaskFunctionT & initF=nullptr ):f(_f)
    {
        if(initF)
            initF();
    }

    ~ScopeGuard()
    {
        if(f)
            f();
    }
    const BasicTaskFunctionT f=nullptr;
};
}
#endif // _HEAD_BASIC_UTILS_