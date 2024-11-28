#ifndef _HEAD_BASIC_UTILS_
#define _HEAD_BASIC_UTILS_
#include <chrono>
#include <functional>
#include <string>

namespace Fundamental
{

struct NonCopyable
{
    NonCopyable()                              = default;
    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

using BasicTaskFunctionT = std::function<void()>;
struct ScopeGuard final : NonCopyable
{
    ScopeGuard(const BasicTaskFunctionT& _f, const BasicTaskFunctionT& initF = nullptr) :
    f(_f)
    {
        if (initF)
            initF();
    }

    ~ScopeGuard()
    {
        if (f)
            f();
    }
    const BasicTaskFunctionT f = nullptr;
};

template <typename T>
struct Singleton : NonCopyable
{
    static T& Instance()
    {
        return instance;
    };

    inline static T instance;
};

namespace Utils
{
     void SetThreadName(const std::string& name);
     std::string BufferToHex(const void * buffer,std::size_t size);
     std::string BufferDumpAscii(const void * buffer,std::size_t size);
}

} // namespace Fundamental
#endif // _HEAD_BASIC_UTILS_