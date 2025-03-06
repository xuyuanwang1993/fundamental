#pragma once
#include "const_vars.h"
#include "io_context_pool.hpp"

#include <memory>

#include "fundamental/application/application.hpp"
#include "fundamental/basic/utils.hpp"
namespace network
{
inline void init_io_context_pool(std::size_t work_threads = std::thread::hardware_concurrency()) {
    network::io_context_pool::s_excutorNums = work_threads;
    network::io_context_pool::Instance().start();
    Fundamental::Application::Instance().exitStarted.Connect([&]() {
        network::io_context_pool::Instance().stop();
    });
    network::io_context_pool::Instance().notify_sys_signal.Connect(
        [](std::error_code code, std::int32_t signo) { Fundamental::Application::Instance().Exit(); });
}

template <typename T>
struct auto_rpc_storage_instance : Fundamental::NonCopyable {
    using HandleType = typename decltype(Fundamental::Application::Instance().exitStarted)::HandleType;
    auto_rpc_storage_instance(std::shared_ptr<T> ptr) : ref_ptr(std::move(ptr)) {
        handle_ = Fundamental::Application::Instance().exitStarted.Connect([&]() { release(); });
    }
    ~auto_rpc_storage_instance() {
        release();
        
        Fundamental::Application::Instance().exitStarted.DisConnect(handle_);
    }
    auto_rpc_storage_instance(auto_rpc_storage_instance&& other) noexcept :
    handle_(std::move(other.handle_)), ref_ptr(std::move(other.ref_ptr)) {
    }

    auto_rpc_storage_instance& operator==(auto_rpc_storage_instance&& other) noexcept {
        handle_ = std::move(other.handle_);
        ref_ptr = std::move(other.ref_ptr);
        return *this;
    }
    decltype(auto) get() {
        return ref_ptr;
    }

    decltype(auto) operator->() {
        return ref_ptr.get();
    }
    void release()
    {
        ref_ptr->release_obj();
    }
private:
    HandleType handle_;
    std::shared_ptr<T> ref_ptr;
};

template <typename ClassType, typename... Args>
inline decltype(auto) make_guard(Args&&... args) {
    return auto_rpc_storage_instance(ClassType::make_shared(std::forward<Args>(args)...));
}
} // namespace network