#ifndef REST_RPC_RPC_SERVER_H_
#define REST_RPC_RPC_SERVER_H_

#include "basic/router.hpp"
#include "connection.h"

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <set>
#include <thread>

#include "basic/rpc_init.hpp"
#include "fundamental/events/event_system.h"

using asio::ip::tcp;

namespace network
{
namespace rpc_service
{
using rpc_conn = std::weak_ptr<connection>;

struct protocal_helper {
    static tcp::endpoint make_endpoint(std::uint16_t port) {
#ifndef RPC_IPV4_ONLY
        return tcp::endpoint(tcp::v6(), port);
#else
        return tcp::endpoint(tcp::v4(), port);
#endif
    }
    static void init_acceptor(tcp::acceptor& acceptor, std::uint16_t port) {
        auto end_point = make_endpoint(port);
        acceptor.open(end_point.protocol());
        acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#ifndef RPC_IPV4_ONLY
        // 关闭 v6_only 选项，允许同时接受 IPv4 和 IPv6 连接
        asio::ip::v6_only v6_option(false);
        acceptor.set_option(v6_option);
#endif
        acceptor.bind(end_point);
        acceptor.listen();
    }
};

class rpc_server : private asio::noncopyable, public std::enable_shared_from_this<rpc_server> {
    friend class connection;

public:
    Fundamental::Signal<void(asio::error_code, string_view)> on_err;
    Fundamental::Signal<void(std::shared_ptr<connection>, std::string)> on_net_err;
    Fundamental::Signal<void()> on_release;

public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<rpc_server>(std::forward<Args>(args)...);
    }
    rpc_server(unsigned short port, size_t timeout_msec = 30000) :
    acceptor_(io_context_pool::Instance().get_io_context()), timeout_msec_(timeout_msec) {
        protocal_helper::init_acceptor(acceptor_, port);
    }
    ~rpc_server() {
        FDEBUG("release rpc_server");
    }

    void start() {
        bool expected_value = false;
        if (!has_started_.compare_exchange_strong(expected_value, true)) return;
        do_accept();
    }

    void stop() {
        release_obj();
    }
    template <bool is_pub = false, typename Function>
    void register_handler(std::string const& name, const Function& f) {
        router_.register_handler<is_pub>(name, f);
    }

    template <bool is_pub = false, typename Function, typename Self>
    void register_handler(std::string const& name, const Function& f, Self* self) {
        router_.register_handler<is_pub>(name, f, self);
    }

    template <typename... Args>
    void publish(std::string key, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(sub_mtx_);
            if (sub_map_.empty()) return;
        }
        auto ret = rpc_service::msgpack_codec::pack(std::forward<Args>(args)...);
        std::string s_data(ret.data(), ret.data() + ret.size());
        forward_msg(std::move(key), std::move(s_data));
    }

    void release_obj() {
        reference_.release();
        bool expected_value = true;
        if (!has_started_.compare_exchange_strong(expected_value, false)) return;
        on_release.Emit();
        std::promise<void> promise;
        asio::post(acceptor_.get_executor(), [this, &promise] {
            try {
                std::error_code ec;
                acceptor_.close(ec);
            } catch (const std::exception& e) {
            }
            promise.set_value();
        });
        promise.get_future().wait();
    }
    // this function will throw when param is invalid
    void enable_ssl(rpc_server_ssl_config ssl_config) {
#ifndef RPC_DISABLE_SSL
        if (ssl_config.certificate_path.empty() || ssl_config.private_key_path.empty() ||
            !std::filesystem::is_regular_file(ssl_config.certificate_path) ||
            !std::filesystem::is_regular_file(ssl_config.private_key_path)) {
            throw std::invalid_argument("rpc_server/ssl need valid certificate and key file");
        }
        if (!ssl_config.tmp_dh_path.empty() && !std::filesystem::is_regular_file(ssl_config.tmp_dh_path)) {
            throw std::invalid_argument("tmp_dh_path is not existed");
        }
        std::swap(ssl_config_, ssl_config);
        if (!ssl_config_.passwd_cb) ssl_config_.passwd_cb = [](std::string) -> std::string { return "123456"; };

        unsigned long ssl_options =
            asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use;
        ssl_context = std::make_unique<asio::ssl::context>(asio::ssl::context::sslv23);
        try {
            ssl_context->set_options(ssl_options);
            ssl_context->set_password_callback(
                [cb = ssl_config_.passwd_cb](std::size_t size, asio::ssl::context_base::password_purpose purpose) {
                    return cb(std::to_string(size) + " " + std::to_string(static_cast<std::size_t>(purpose)));
                });

            ssl_context->use_certificate_chain_file(ssl_config_.certificate_path);
            ssl_context->use_private_key_file(ssl_config_.private_key_path, asio::ssl::context::pem);
            if (!ssl_config_.tmp_dh_path.empty()) ssl_context->use_tmp_dh_file(ssl_config_.tmp_dh_path);
        } catch (const std::exception& e) {
            throw std::invalid_argument(std::string("load ssl config failed ") + e.what());
        }

#endif
    }
    void enable_data_proxy(network::proxy::ProxyManager* manager) {
        proxy_manager = manager;
    }

private:
    void do_accept() {

        acceptor_.async_accept(
            io_context_pool::Instance().get_io_context(),
            [this, ptr = shared_from_this()](asio::error_code ec, asio::ip::tcp::socket socket) {
                if (!reference_.is_valid()) {
                    FDEBUG("instance {:p} has alread release", (void*)this);
                    return;
                }
                if (!acceptor_.is_open()) {
                    return;
                }

                if (ec) {
                    // maybe system error... ignored
                } else {
                    auto new_conn = connection::make_shared(std::move(socket), timeout_msec_, router_);
                    new_conn->on_new_subscriber_added.Connect([this](std::string key, std::weak_ptr<connection> conn) {
                        std::unique_lock<std::mutex> lock(sub_mtx_);
                        sub_map_.emplace(std::move(key), conn);
                    });
                    if (on_net_err) {
                        new_conn->on_net_err_.Connect(on_net_err);
                    }
                    auto release_handle = on_release.Connect([con = new_conn->weak_from_this()]() {
                        auto ptr = con.lock();
                        if (ptr) ptr->close();
                    });
                    new_conn->on_connection_closed.Connect(
                        [release_handle, this]() { on_release.DisConnect(release_handle); });
                    new_conn->on_publish_msg.Connect(
                        [this](std::string key, std::string data) { forward_msg(std::move(key), std::move(data)); });
                    new_conn->on_subscribers_removed.Connect(
                        [this](const std::unordered_set<std::string>& keys, std::weak_ptr<connection> conn) {
                            std::unique_lock<std::mutex> lock(sub_mtx_);
                            for (auto& key : keys) {
                                auto range = sub_map_.equal_range(key);
                                for (auto it = range.first; it != range.second;) {
                                    if (it->second.lock() == conn.lock()) {
                                        it = sub_map_.erase(it);
                                        break;
                                    } else {
                                        ++it;
                                    }
                                }
                            }
                        });
#ifndef RPC_DISABLE_SSL
                    if (ssl_context) {
                        new_conn->enable_ssl(*ssl_context);
                    }
#endif
                    std::int64_t id = 0;
                    // increase conn_id_ and return old value
                    while (true) {
                        id = conn_id_.load();
                        if (conn_id_.compare_exchange_strong(id, id + 1)) break;
                    }
                    new_conn->set_conn_id(id);
                    new_conn->config_proxy_manager(proxy_manager);
                    new_conn->start();
                    FDEBUG("start connection {:p} -> {}", (void*)(new_conn.get()), id);
                }

                do_accept();
            });
    }

    void forward_msg(std::string key, std::string data) {
        std::unique_lock<std::mutex> lock(sub_mtx_);
        auto range = sub_map_.equal_range(key);
        if (range.first != range.second) {
            for (auto it = range.first; it != range.second; ++it) {
                auto conn = it->second.lock();
                if (conn == nullptr || conn->has_closed()) {
                    continue;
                }
                conn->publish(key, data);
            }
        } else {
            if (!sub_map_.empty())
                on_err(asio::error::make_error_code(asio::error::invalid_argument),
                       "The subscriber of the key: " + key + " does not exist.");
        }
    }
    rpc_data_reference reference_;
    std::atomic_bool has_started_ = false;
    tcp::acceptor acceptor_;
    std::size_t timeout_msec_ = 0;

    std::atomic<std::int64_t> conn_id_ = 0;

    std::unordered_multimap<std::string, std::weak_ptr<connection>> sub_map_;
    std::mutex sub_mtx_;

    router router_;
#ifndef RPC_DISABLE_SSL
    std::unique_ptr<asio::ssl::context> ssl_context = nullptr;
    rpc_server_ssl_config ssl_config_;
#endif
    // proxy
    network::proxy::ProxyManager* proxy_manager = nullptr;
};
} // namespace rpc_service
  // namespace rpc_service
} // namespace network

#endif // REST_RPC_RPC_SERVER_H_
