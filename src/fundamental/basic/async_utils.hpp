#pragma once
#include "fundamental/basic/log.h"

#include <chrono>
#include <future>
#include <memory>

namespace Fundamental
{

template <typename T>
inline decltype(auto) MakeSharedPromise() {
    return std::make_shared<std::promise<T>>();
}
struct AsyncTaskObjectGuard {
    class AsyncTaskObjectGuardHandle {
        friend struct AsyncTaskObjectGuard;

    public:
        bool Valid() const {
            return __g__ && *__g__;
        }
        operator bool() const {
            return Valid();
        }
        bool operator!() const {
            return !Valid();
        }
        AsyncTaskObjectGuardHandle(std::shared_ptr<bool> guard_ref) : __g__(guard_ref) {
        }
        AsyncTaskObjectGuardHandle(const AsyncTaskObjectGuardHandle& other) : __g__(other.__g__) {
        }
        AsyncTaskObjectGuardHandle(AsyncTaskObjectGuardHandle&& other) noexcept : __g__(std::move(other.__g__)) {
        }
        AsyncTaskObjectGuardHandle& operator==(const AsyncTaskObjectGuardHandle& other) {
            __g__ = other.__g__;
            return *this;
        }
        AsyncTaskObjectGuardHandle& operator==(AsyncTaskObjectGuardHandle&& other) noexcept {
            __g__ = std::move(other.__g__);
            return *this;
        }

    private:
        std::shared_ptr<bool> __g__;
    };
    AsyncTaskObjectGuard(std::size_t max_wait_msecs = 10) :
    max_wait_msecs(max_wait_msecs), __async_task_release_guard__(std::make_shared<bool>(false)) {
    }
    ~AsyncTaskObjectGuard() {
        *(__async_task_release_guard__.get()) = true;
        auto rest_wait_msecs                  = max_wait_msecs;
        // no other reference
        while (__async_task_release_guard__.use_count() != 1) {
            if (rest_wait_msecs > 0) {
                --rest_wait_msecs;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    decltype(auto) AddReference() {
        return AsyncTaskObjectGuardHandle(__async_task_release_guard__);
    }

private:
    const std::size_t max_wait_msecs = 10;
    std::shared_ptr<bool> __async_task_release_guard__;
};
} // namespace Fundamental