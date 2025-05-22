#pragma once

#include "io_context_pool.hpp"
#include "use_asio.hpp"
#include <functional>
#if TARGET_PLATFORM_LINUX
    #include <netinet/tcp.h>
#endif

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
    bool release() {
        auto expected_value = false;
        if (__has_released.compare_exchange_strong(expected_value, true)) {
            notify_release.Emit();
            return true;
        }
        return false;
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
    bool enable_no_ssl = true;
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
    auto_network_storage_instance() = default;
    ~auto_network_storage_instance() {
        Fundamental::Application::Instance().exitStarted.DisConnect(handle_);
        release();
    }
    auto_network_storage_instance(auto_network_storage_instance&& other) noexcept :
    auto_network_storage_instance(std::move(other.ref_ptr)) {
    }

    auto_network_storage_instance& operator=(auto_network_storage_instance&& other) noexcept {
        release();
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
        ref_ptr = nullptr;
    }

private:
    std::shared_ptr<T> ref_ptr;
    HandleType handle_;
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
// ss -to | grep -i keepalive 可以查看是否启用了选项
inline void enable_tcp_keep_alive(asio::ip::tcp::socket& socket,
                                  bool enable               = true,
                                  std::int32_t idle_sec     = 30,
                                  std::int32_t interval_sec = 5,
                                  std::int32_t max_probes   = 3) {
    asio::error_code ec;
    socket.set_option(asio::socket_base::keep_alive(enable), ec);
#if TARGET_PLATFORM_LINUX
    if (enable) {
        setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPIDLE, &idle_sec, sizeof(idle_sec));
        setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPINTVL, &interval_sec, sizeof(interval_sec));
        setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPCNT, &max_probes, sizeof(max_probes));
    }
#endif
}
} // namespace network
