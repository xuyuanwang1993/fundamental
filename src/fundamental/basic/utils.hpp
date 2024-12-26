#ifndef _HEAD_BASIC_UTILS_
#define _HEAD_BASIC_UTILS_
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
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
        static T instance;
        return instance;
    };

protected:
    Singleton() {
    }
};

namespace Utils {
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