#ifndef _HEAD_BASIC_UTILS_
#define _HEAD_BASIC_UTILS_
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>

#define F_UNUSED(x) (void)x

namespace Fundamental {

struct NonCopyable {
    NonCopyable()                              = default;
    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

struct NonMovable {
    NonMovable()                         = default;
    NonMovable(NonMovable&&)             = delete;
    NonMovable& operator=(NonCopyable&&) = delete;
};

using BasicTaskFunctionT = std::function<void()>;
struct ScopeGuard final : NonCopyable {
    ScopeGuard(const BasicTaskFunctionT& _f, const BasicTaskFunctionT& initF = nullptr) : f(_f) {
        if (initF) initF();
    }

    ~ScopeGuard() {
        if (f) f();
    }
    const BasicTaskFunctionT f = nullptr;
};

template <typename T>
struct Singleton : NonCopyable, NonMovable {
    static T& Instance() {
        // notice we can't use a value type here
        // sometimes we will access another static instance to release some runtime resources
        //  or access a logger instance
        //  when we access another static instance in the desturct function,
        // The memory access safety of this behavior cannot be guaranteed
        static T* instance = new T();
        return *instance;
    };

protected:
    Singleton() {
    }
    ~Singleton() {
    }
};

namespace Utils {
using fpid_t = std::uint32_t;
fpid_t GetProcessId();
void SetThreadName(const std::string& name);
std::string BufferToHex(const void* buffer, std::size_t size);
std::string BufferDumpAscii(const void* buffer, std::size_t size);
struct NetworkInfo {
    std::string ifName;
    std::string ipv4;
    std::string mac;
    bool isLoopback = false;
};
std::unordered_map<std::string, NetworkInfo> GetLocalNetInformation();
} // namespace Utils

} // namespace Fundamental
#endif // _HEAD_BASIC_UTILS_