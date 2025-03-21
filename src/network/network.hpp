#pragma once

#include "io_context_pool.hpp"
#include "use_asio.hpp"
#include <functional>
namespace network
{
static constexpr std::size_t kSslPreReadSize = 3;
struct network_data_reference {
    Fundamental::Signal<void()> notify_release;
    bool is_valid() const {
        return !__has_released;
    }
    operator bool() const {
        return is_valid();
    }
    bool operator!() const {
        return !is_valid();
    }
    void release() {
        auto expected_value = false;
        if (__has_released.compare_exchange_strong(expected_value, true)) {
            notify_release.Emit();
        }
    }
    std::atomic_bool __has_released = false;
};

struct network_server_ssl_config {
    std::function<std::string(std::string)> passwd_cb;
    std::string certificate_path;
    std::string private_key_path;
    std::string tmp_dh_path;
    std::string ca_certificate_path;
    bool verify_client = true;
    bool disable_ssl   = false;
};

struct network_client_ssl_config {
    std::string certificate_path;
    std::string private_key_path;
    std::string ca_certificate_path;
    bool verify_org  = true;
    bool disable_ssl = false;
};

template <typename T>
struct auto_network_storage_instance : Fundamental::NonCopyable {
    using HandleType = typename decltype(Fundamental::Application::Instance().exitStarted)::HandleType;
    auto_network_storage_instance(std::shared_ptr<T> ptr) : ref_ptr(std::move(ptr)) {
        handle_ = Fundamental::Application::Instance().exitStarted.Connect([&]() { release(); });
    }
    ~auto_network_storage_instance() {
        release();

        Fundamental::Application::Instance().exitStarted.DisConnect(handle_);
    }
    auto_network_storage_instance(auto_network_storage_instance&& other) noexcept :
    handle_(std::move(other.handle_)), ref_ptr(std::move(other.ref_ptr)) {
    }

    auto_network_storage_instance& operator=(auto_network_storage_instance&& other) noexcept {
        release();
        Fundamental::Application::Instance().exitStarted.DisConnect(handle_);

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
    void release() {
        if (ref_ptr) ref_ptr->release_obj();
    }

private:
    HandleType handle_;
    std::shared_ptr<T> ref_ptr;
};

template <typename ClassType, typename... Args>
inline decltype(auto) make_guard(Args&&... args) {
    return auto_network_storage_instance(ClassType::make_shared(std::forward<Args>(args)...));
}

struct protocal_helper {
    static asio::ip::tcp::endpoint make_endpoint(std::uint16_t port) {
#ifndef NETWORK_IPV4_ONLY
        return asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port);
#else
        return asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port);
#endif
    }
    static void init_acceptor(asio::ip::tcp::acceptor& acceptor, std::uint16_t port) {
        auto end_point = make_endpoint(port);
        acceptor.open(end_point.protocol());
        acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#ifndef NETWORK_IPV4_ONLY
        // 关闭 v6_only 选项，允许同时接受 IPv4 和 IPv6 连接
        asio::ip::v6_only v6_option(false);
        acceptor.set_option(v6_option);
#endif
        acceptor.bind(end_point);
        acceptor.listen();
    }
};
} // namespace network
