#pragma once
#include "basic/client_util.hpp"
#include "basic/const_vars.h"
#include "basic/io_context_pool.hpp"
#include "basic/md5.hpp"
#include "basic/meta_util.hpp"
#include "basic/rpc_client_proxy.hpp"
#include "basic/use_asio.hpp"

#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>

#include "fundamental/basic/log.h"

namespace network
{
namespace rpc_service
{
class req_result {
public:
    req_result() = default;
    req_result(string_view data) : data_(data.data(), data.length()) {
    }
    bool success() const {
        return !has_error(data_);
    }

    template <typename T>
    T as() {
        if (has_error(data_)) {
            std::string err_msg = data_.empty() ? data_ : get_error_msg(data_);
            throw std::logic_error(err_msg);
        }

        return get_result<T>(data_);
    }

    void as() {
        if (has_error(data_)) {
            std::string err_msg = data_.empty() ? data_ : get_error_msg(data_);
            throw std::logic_error(err_msg);
        }
    }

private:
    std::string data_;
};

template <typename T>
struct future_result {
    uint64_t id;
    std::future<T> future;
    template <class Rep, class Per>
    std::future_status wait_for(const std::chrono::duration<Rep, Per>& rel_time) {
        return future.wait_for(rel_time);
    }

    T get() {
        return future.get();
    }
};

enum class CallModel
{
    future,
    callback
};
const constexpr auto FUTURE = CallModel::future;

const constexpr size_t DEFAULT_TIMEOUT = 5000; // milliseconds
enum rpc_client_ssl_level
{
    rpc_client_ssl_level_none,
    rpc_client_ssl_level_optional,
    rpc_client_ssl_level_required
};

// call these interface not in io thread
class ClientStreamReadWriter {
public:
    ClientStreamReadWriter(rpc_client& client);
    ~ClientStreamReadWriter();
    template <typename T>
    bool Read(T& request, std::size_t max_wait_ms = 5000);
    template <typename U>
    bool Write(U&& response);
    bool WriteDone();
    std::error_code Finish(std::size_t max_wait_ms = 5000);
    std::error_code GetLastError() const;

private:
    void read_head();
    void read_body();
    void set_status(rpc_stream_data_status status, std::error_code ec);
    void handle_write();

private:
    std::mutex mutex;
    std::error_code last_err_;
    rpc_stream_packet read_packet_buffer;
    std::atomic<rpc_stream_data_status> last_data_status_ = rpc_stream_data_status::rpc_stream_none;

    rpc_client& client_;
    std::condition_variable cv_;
    std::deque<std::vector<std::uint8_t>> response_cache_;
    std::deque<rpc_stream_packet> write_cache_;
};

class rpc_client : private asio::noncopyable {
    friend class ClientStreamReadWriter;

public:
    inline static std::function<asio::io_context&()> s_io_context_cb = []() -> decltype(auto) {
        return network::io_context_pool::Instance().get_io_context();
    };

public:
    rpc_client() :
    ios_(s_io_context_cb()), socket_(ios_), reconnect_delay_timer_(ios_), deadline_(ios_), body_(INIT_BUF_SIZE) {
    }

    rpc_client(std::string host, unsigned short port) :
    ios_(s_io_context_cb()), socket_(ios_), host_(std::move(host)), port_(port), reconnect_delay_timer_(ios_),
    deadline_(ios_), body_(INIT_BUF_SIZE) {
    }

    ~rpc_client() {
        stop();
    }

    void set_reconnect_delay(size_t milliseconds) {
        reconnect_delay_ms = milliseconds;
    }

    void set_reconnect_count(int reconnect_count) {
        reconnect_cnt_ = reconnect_count;
    }

    bool connect(size_t timeout = 3) {
        if (has_connected_) return true;

        assert(port_ != 0);
        async_connect();
        return wait_conn(timeout);
    }

    bool connect(const std::string& host, unsigned short port, size_t timeout = 3) {
        if (port_ == 0) {
            host_ = host;
            port_ = port;
        }

        return connect(timeout);
    }

    void async_connect(const std::string& host, unsigned short port) {
        if (port_ == 0) {
            host_ = host;
            port_ = port;
        }

        async_connect();
    }

    bool wait_conn(size_t timeout) {
        if (has_connected_) {
            return true;
        }

        has_wait_ = true;
        std::unique_lock<std::mutex> lock(conn_mtx_);
        [[maybe_unused]] bool result =
            conn_cond_.wait_for(lock, std::chrono::seconds(timeout), [this] { return has_connected_.load(); });
        has_wait_ = false;
        return has_connected_;
    }

    void enable_auto_reconnect(bool enable = true) {
        enable_reconnect_ = enable;
        reconnect_cnt_    = std::numeric_limits<int>::max();
    }

    void enable_auto_heartbeat(bool enable = true) {
        if (enable) {
            reset_deadline_timer(5);
        } else {
            deadline_.cancel();
        }
    }

    void set_proxy(std::shared_ptr<RpcClientProxyInterface> proxy) {
        proxy_interface = proxy;
    }

    void update_addr(const std::string& host, unsigned short port) {
        host_ = host;
        port_ = port;
    }

    void close() {

        if (!has_connected_) return;
        has_connected_ = false;
        asio::error_code ec;
#ifndef RPC_DISABLE_SSL
        if (ssl_stream_) {
            ssl_stream_->shutdown(ec);
            ssl_stream_ = nullptr;
        }
#endif
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        clear_cache();
    }

    void set_error_callback(std::function<void(asio::error_code)> f) {
        err_cb_ = std::move(f);
    }

    uint64_t reqest_id() {
        return temp_req_id_;
    }

    bool has_connected() const {
        return has_connected_;
    }

    // sync call
    template <size_t TIMEOUT, typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value>::type call(const std::string& rpc_name, Args&&... args) {
        auto future_result = async_call<FUTURE>(rpc_name, std::forward<Args>(args)...);
        auto status        = future_result.wait_for(std::chrono::milliseconds(TIMEOUT));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            throw std::out_of_range("timeout or deferred");
        }

        future_result.get().as();
    }

    template <typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value>::type call(const std::string& rpc_name, Args&&... args) {
        call<DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }

    template <size_t TIMEOUT, typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type call(const std::string& rpc_name, Args&&... args) {
        auto future_result = async_call<FUTURE>(rpc_name, std::forward<Args>(args)...);
        auto status        = future_result.wait_for(std::chrono::milliseconds(TIMEOUT));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            throw std::out_of_range("rpc timeout or deferred");
        }

        return future_result.get().template as<T>();
    }

    template <typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type call(const std::string& rpc_name, Args&&... args) {
        return call<DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }

    template <CallModel model, typename... Args>
    future_result<req_result> async_call(const std::string& rpc_name, Args&&... args) {
        if (has_upgrade) throw std::runtime_error("client has already upgrade");
        auto p                         = std::make_shared<std::promise<req_result>>();
        std::future<req_result> future = p->get_future();

        uint64_t fu_id = 0;
        {
            std::unique_lock<std::mutex> lock(cb_mtx_);
            fu_id_++;
            fu_id = fu_id_;
            future_map_.emplace(fu_id, std::move(p));
        }

        rpc_service::msgpack_codec codec;
        auto ret = codec.pack(std::forward<Args>(args)...);
        write(fu_id, request_type::rpc_req, std::move(ret), MD5::MD5Hash32(rpc_name.data()));
        return future_result<req_result> { fu_id, std::move(future) };
    }

    template <size_t TIMEOUT = DEFAULT_TIMEOUT, typename... Args>
    void async_call(const std::string& rpc_name,
                    std::function<void(asio::error_code, string_view)> cb,
                    Args&&... args) {
        if (has_upgrade) throw std::runtime_error("client has already upgrade");
        if (!has_connected_) {
            if (cb) cb(asio::error::make_error_code(asio::error::not_connected), "not connected");
            return;
        }

        uint64_t cb_id = 0;
        {
            std::unique_lock<std::mutex> lock(cb_mtx_);
            callback_id_++;
            callback_id_ |= (uint64_t(1) << 63);
            cb_id     = callback_id_;
            auto call = std::make_shared<call_t>(ios_, std::move(cb), TIMEOUT);
            call->start_timer();
            callback_map_.emplace(cb_id, call);
        }

        rpc_service::msgpack_codec codec;
        auto ret = codec.pack(std::forward<Args>(args)...);
        write(cb_id, request_type::rpc_req, std::move(ret), MD5::MD5Hash32(rpc_name.data()));
    }

    void stop() {
        std::promise<void> promise;
        asio::post(ios_, [this, &promise] {
            close();
            stop_client_ = true;
            reconnect_delay_timer_.cancel();
            deadline_.cancel();
            promise.set_value();
        });
        promise.get_future().wait();
    }

    template <typename Func>
    future_result<req_result> subscribe(std::string key, Func f) {
        if (has_upgrade) throw std::runtime_error("client has already upgrade");
        {
            std::unique_lock<std::mutex> lock(sub_mtx_);
            auto it = sub_map_.find(key);
            if (it != sub_map_.end()) {
                FASSERT(false && "duplicated subscribe");
                return {};
            }
            sub_map_.emplace(key, std::move(f));
        }

        return post_future_call(request_type::rpc_subscribe, key);
    }

    future_result<req_result> unsubscribe(std::string key) {
        if (has_upgrade) throw std::runtime_error("client has already upgrade");
        {
            std::unique_lock<std::mutex> lock(sub_mtx_);
            auto it = sub_map_.find(key);
            if (it == sub_map_.end()) {
                FASSERT(false && "subscribe key not found");
                return {};
            }
            sub_map_.erase(key);
        }

        return post_future_call(request_type::rpc_subscribe, key);
    }

    template <typename... Args>
    future_result<req_result> publish(std::string key, Args&&... args) {
        // convert to format string data
        rpc_service::msgpack_codec codec;
        auto ret = codec.pack(std::forward<Args>(args)...);
        return post_future_call(request_type::rpc_publish, key, std::string(ret.data(), ret.data() + ret.size()));
    }

    void enable_ssl(const std::string& ser_pem_path,
                    rpc_client_ssl_level enable_ssl_level = rpc_client_ssl_level::rpc_client_ssl_level_required) {
#ifndef RPC_DISABLE_SSL
        pem_path  = ser_pem_path;
        ssl_level = enable_ssl_level;
#endif
    }

    std::shared_ptr<ClientStreamReadWriter> upgrade_to_stream(std::string_view rpc_name,
                                                              std::size_t timeout_msec = 5000) {
        if (has_upgrade) throw std::runtime_error("client has already upgrade");
        auto p                         = std::make_shared<std::promise<req_result>>();
        std::future<req_result> future = p->get_future();

        uint64_t fu_id = 0;
        {
            std::unique_lock<std::mutex> lock(cb_mtx_);
            fu_id_++;
            fu_id = fu_id_;
            future_map_.emplace(fu_id, std::move(p));
        }
        write(fu_id, request_type::rpc_stream, {}, MD5::MD5Hash32(rpc_name.data()));
        std::shared_ptr<ClientStreamReadWriter> ret;
        try {
            if (timeout_msec > 0)
                future.wait_for(std::chrono::milliseconds(timeout_msec));
            else {
                future.wait();
            }
            ret = std::make_shared<ClientStreamReadWriter>(*this);
        } catch (const std::exception& e) {
#ifdef DEBUG
            std::cout << "upgrade_to_stream: failed for " << e.what();
#endif
        }
        return ret;
    }

private:
    template <typename... Args>
    future_result<req_result> post_future_call(request_type call_type, Args&&... args) {
        auto p                         = std::make_shared<std::promise<req_result>>();
        std::future<req_result> future = p->get_future();

        uint64_t fu_id = 0;
        {
            std::unique_lock<std::mutex> lock(cb_mtx_);
            fu_id_++;
            fu_id = fu_id_;
            future_map_.emplace(fu_id, std::move(p));
        }

        rpc_service::msgpack_codec codec;
        auto ret = codec.pack(std::forward<Args>(args)...);
        write(fu_id, call_type, std::move(ret), 0);
        return future_result<req_result> { fu_id, std::move(future) };
    }

    bool is_ssl() const {
#ifndef RPC_DISABLE_SSL
        return ssl_level != rpc_client_ssl_level_none;
#else
        return false;
#endif
    }
    void async_connect() {
        assert(port_ != 0);
        auto addr = asio::ip::make_address(host_);
        socket_.async_connect({ addr, port_ }, [this](const asio::error_code& ec) {
            if (has_connected_ || stop_client_) {
                return;
            }

            if (ec) {

                has_connected_ = false;

                if (reconnect_cnt_ <= 0) {
                    return;
                }

                if (reconnect_cnt_ > 0) {
                    reconnect_cnt_--;
                }
                async_reconnect();
            } else {
                handle_connect_success();
            }
        });
    }
    void perform_proxy() {
        proxy_interface->init([this]() { handle_transfer_ready(); },
                              [this](const asio::error_code& ec) { error_callback(ec); }, &socket_);
        proxy_interface->perform();
    }
    void handle_connect_success() {
        if (proxy_interface)
            perform_proxy();
        else {
            handle_transfer_ready();
        }
    }
    void handle_transfer_ready() {
        if (is_ssl()) {
            ssl_handshake();
        } else {
            rpc_protocal_ready();
        }
    }
    void rpc_protocal_ready() {
        has_connected_ = true;
        do_read();
        resend_subscribe();
        if (has_wait_) conn_cond_.notify_one();
    }
    void async_reconnect() {
        if (stop_client_) {
            return;
        }
        reset_socket();
        reconnect_delay_timer_.expires_after(std::chrono::milliseconds(reconnect_delay_ms));
        reconnect_delay_timer_.async_wait([this](const asio::error_code& ec) {
            if (!ec) {
                async_connect();
                return;
            }

            error_callback(ec);
        });
    }

    void reset_deadline_timer(size_t timeout) {
        if (stop_client_) {
            return;
        }

        deadline_.expires_after(std::chrono::seconds(timeout));
        deadline_.async_wait([this, timeout](const asio::error_code& ec) {
            if (!ec) {
                if (has_connected_) {
                    write(0, request_type::rpc_heartbeat, rpc_service::rpc_buffer_type(0), 0);
                }
            }

            reset_deadline_timer(timeout);
        });
    }

    void write(std::uint64_t req_id, request_type type, rpc_service::rpc_buffer_type&& message, uint32_t func_id) {
        size_t size = message.size();
        assert(size < MAX_BUF_LEN);
        client_message_type msg { req_id, type, std::move(message), func_id };

        std::unique_lock<std::mutex> lock(write_mtx_);
        outbox_.emplace_back(std::move(msg));
        if (outbox_.size() > 1) {
            // outstanding async_write
            return;
        }

        write();
    }

    void write() {
        auto& msg   = outbox_[0];
        write_size_ = (uint32_t)msg.content.size();
        std::array<asio::const_buffer, 2> write_buffers;
        rpc_header { RPC_MAGIC_NUM, msg.req_type, write_size_, msg.req_id, msg.func_id }.Serialize(write_head_,
                                                                                                   kRpcHeadLen);
        write_buffers[0] = asio::buffer(write_head_, kRpcHeadLen);
        write_buffers[1] = asio::buffer((char*)msg.content.data(), write_size_);

        async_write_buffers(std::move(write_buffers), [this](const asio::error_code& ec, const size_t length) {
            if (ec) {
                has_connected_ = false;
                close();
                error_callback(ec);

                return;
            }

            std::unique_lock<std::mutex> lock(write_mtx_);
            if (outbox_.empty()) {
                return;
            }
#ifdef RPC_VERBOSE
            auto& msg = outbox_[0];
            FDEBUG("client write header:{} body:{}", Fundamental::Utils::BufferToHex(write_head_, kRpcHeadLen),
                   Fundamental::Utils::BufferToHex(msg.content.data(), msg.content.size(),140));
#endif
            outbox_.pop_front();

            if (!outbox_.empty()) {
                // more messages to send
                this->write();
            }
        });
    }

    void do_read() {
        async_buffer_read({ asio::buffer(head_, kRpcHeadLen) },
                          [this](const asio::error_code& ec, const size_t length) {
                              if (!socket_.is_open()) {
                                  has_connected_ = false;
                                  return;
                              }

                              if (!ec) {
#ifdef RPC_VERBOSE
                                  FDEBUG("client read head: {}", Fundamental::Utils::BufferToHex(head_, kRpcHeadLen));
#endif
                                  rpc_header header;
                                  header.DeSerialize(head_, kRpcHeadLen);
                                  const uint32_t body_len = header.body_len;
                                  if (body_len > 0 && body_len < MAX_BUF_LEN) {
                                      if (body_.size() < body_len) {
                                          body_.resize(body_len);
                                      }
                                      read_body(header.req_id, header.req_type, body_len);
                                      return;
                                  }

                                  if (body_len == 0 || body_len > MAX_BUF_LEN) {
                                      close();
                                      error_callback(asio::error::make_error_code(asio::error::message_size));
                                      return;
                                  }
                              } else {
                                  close();
                                  error_callback(ec);
                              }
                          });
    }

    void read_body(std::uint64_t req_id, request_type req_type, size_t body_len) {
        async_buffer_read({ asio::buffer(body_.data(), body_len) }, [this, req_id, req_type, body_len](
                                                                        asio::error_code ec, std::size_t length) {
            if (!socket_.is_open()) {
                call_back(req_id, asio::error::make_error_code(asio::error::connection_aborted), {});
                return;
            }

            if (!ec) {
#ifdef RPC_VERBOSE
                FDEBUG("client read body: {}", Fundamental::Utils::BufferToHex(body_.data(), body_len,140));
#endif
                // entier body
                if (req_type == request_type::rpc_res) {
                    call_back(req_id, ec, { body_.data(), body_len });
                    do_read();
                } else if (req_type == request_type::rpc_publish) {
                    callback_sub(ec, { body_.data(), body_len });
                    do_read();
                } else if (req_type == request_type::rpc_stream) {
                    call_back(req_id, ec, { body_.data(), body_len });
                    has_upgrade = true;
                } else {
                    FWARN("invalid req_type failed {}", (std::int32_t)req_type);
                    close();
                    error_callback(asio::error::make_error_code(asio::error::invalid_argument));
                    return;
                }
            } else {
                FWARN("read failed {}", ec.message());
                has_connected_ = false;
                call_back(req_id, ec, {});
                close();
                error_callback(ec);
            }
        });
    }

    void resend_subscribe() {
        if (sub_map_.empty()) return;
        std::vector<std::string> sub_keys;
        {
            std::unique_lock<std::mutex> lock(sub_mtx_);
            for (auto& item : sub_map_) {
                sub_keys.push_back(item.first);
            }
        }
        for (auto& key : sub_keys) {
            post_future_call(request_type::rpc_subscribe, key);
        }
    }

    void call_back(uint64_t req_id, const asio::error_code& ec, string_view data) {

        temp_req_id_ = req_id;
        auto cb_flag = req_id >> 63;
        if (cb_flag) {
            std::shared_ptr<call_t> cl = nullptr;
            {
                std::unique_lock<std::mutex> lock(cb_mtx_);
                auto iter = callback_map_.find(req_id);
                FASSERT(iter != callback_map_.end(), "internal server error invalid req_id {}", req_id);
                cl = iter->second;
                FASSERT(cl != nullptr, "internal error");
                callback_map_.erase(iter);
            }

            if (!cl->has_timeout()) {
                cl->cancel();
                cl->callback(ec, data);
            } else {
                cl->callback(asio::error::make_error_code(asio::error::timed_out), {});
            }
        } else {
            std::shared_ptr<std::promise<req_result>> req_p = nullptr;
            {
                std::unique_lock<std::mutex> lock(cb_mtx_);
                auto iter = future_map_.find(req_id);
                FASSERT(iter != future_map_.end(), "internal server error invalid req_id {}", req_id);
                req_p = iter->second;
                FASSERT(req_p != nullptr, "internal error");
                future_map_.erase(iter);
            }

            if (ec) {
                req_p->set_value(req_result { "" });
                return;
            }
            req_p->set_value(req_result { data });
        }
    }

    void callback_sub(const asio::error_code& ec, string_view result) {
        rpc_service::msgpack_codec codec;
        try {
            auto tp = codec.unpack_tuple<std::tuple<int, std::string, std::string>>(result.data(), result.size());
            [[maybe_unused]] auto code = std::get<0>(tp);
            auto& key                  = std::get<1>(tp);
            auto& data                 = std::get<2>(tp);
            decltype(sub_map_)::iterator::value_type::second_type cb;
            {
                std::unique_lock<std::mutex> lock(sub_mtx_);
                auto it = sub_map_.find(key);
                if (it == sub_map_.end()) {
                    return;
                }
                cb = it->second;
            }
            cb(data);
        } catch (const std::exception& /*ex*/) {
            error_callback(asio::error::make_error_code(asio::error::invalid_argument));
        }
    }

    void clear_cache() {
        {
            std::unique_lock<std::mutex> lock(write_mtx_);
            while (!outbox_.empty()) {
                outbox_.pop_front();
            }
        }

        {
            std::unique_lock<std::mutex> lock(cb_mtx_);
            auto err = error::make_error_code(error::rpc_errors::rpc_failed);
            for (auto& item : callback_map_) {
                item.second->callback(err, "");
            }
            callback_map_.clear();
            for (auto& item : future_map_) {
                item.second->set_exception(std::make_exception_ptr(std::runtime_error("rpc client closed")));
            }
            future_map_.clear();
        }
    }

    void reset_socket() {
        asio::error_code igored_ec;
#ifndef RPC_DISABLE_SSL
        if (ssl_stream_) {
            ssl_stream_->shutdown(igored_ec);
            ssl_stream_ = nullptr;
        }
#endif
        socket_.close(igored_ec);
        socket_ = decltype(socket_)(ios_);
    }

    class call_t : asio::noncopyable, public std::enable_shared_from_this<call_t> {
    public:
        call_t(asio::io_context& ios, std::function<void(asio::error_code, string_view)> cb, size_t timeout) :
        timer_(ios), cb_(std::move(cb)), timeout_(timeout) {
        }

        void start_timer() {
            if (timeout_ == 0) {
                return;
            }

            timer_.expires_after(std::chrono::milliseconds(timeout_));
            auto self = this->shared_from_this();
            timer_.async_wait([this, self](asio::error_code ec) {
                if (ec) {
                    return;
                }

                has_timeout_ = true;
            });
        }

        void callback(asio::error_code ec, string_view data) {
            cb_(ec, data);
        }

        bool has_timeout() const {
            return has_timeout_;
        }

        void cancel() {
            if (timeout_ == 0) {
                return;
            }

            timer_.cancel();
        }

    private:
        asio::steady_timer timer_;
        std::function<void(asio::error_code, string_view)> cb_;
        size_t timeout_;
        bool has_timeout_ = false;
    };

    void error_callback(const asio::error_code& ec) {
        if (err_cb_) {
            err_cb_(ec);
        }

        if (enable_reconnect_) {
            async_reconnect();
        }
    }

    void set_default_error_cb() {
        err_cb_ = [this](asio::error_code) { async_connect(); };
    }
    bool verify_certificate(bool preverified, asio::ssl::verify_context& ctx) {
        // The verify callback can be used to check whether the certificate that is
        // being presented is valid for the peer. For example, RFC 2818 describes
        // the steps involved in doing this for HTTPS. Consult the OpenSSL
        // documentation for more details. Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // In this example we will simply print the certificate's subject name.
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
#ifdef RPC_DEBUG
        std::cout << "[rpc] ssl Verifying:" << subject_name << "\n";
#endif
        return preverified;
    }
    void ssl_handshake() {
#ifndef RPC_DISABLE_SSL
        asio::ssl::context ssl_context(asio::ssl::context::sslv23);
        ssl_context.set_default_verify_paths();
        asio::error_code ec;
        ssl_context.set_options(asio::ssl::context::default_workarounds, ec);
        ssl_context.load_verify_file(pem_path, ec);
        if (ec) {
            if (ssl_level == rpc_client_ssl_level_required) {
                close();
                error_callback(ec);
                return;
            }
        } else {
            ssl_level = rpc_client_ssl_level_required;
        }

        ssl_stream_ = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket&>>(socket_, ssl_context);
        if (ssl_level == rpc_client_ssl_level_required) {
            ssl_stream_->set_verify_mode(asio::ssl::verify_peer);
            ssl_stream_->set_verify_callback(
                std::bind(&rpc_client::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));
        } else {
            ssl_stream_->set_verify_mode(asio::ssl::verify_none);
        }
        ssl_stream_->async_handshake(asio::ssl::stream_base::client, [this](const asio::error_code& ec) {
            if (!ec) {
                rpc_protocal_ready();
            } else {
                FDEBUG("perform client ssl handshake failed {}", ec.message());
                close();
                error_callback(ec);
            }
        });
#endif
    }

    template <typename Handler>
    void async_buffer_read(std::vector<asio::mutable_buffer> buffers, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            asio::async_read(*ssl_stream_, std::move(buffers), std::move(handler));
#endif
        } else {
            asio::async_read(socket_, std::move(buffers), std::move(handler));
        }
    }

    template <typename BufferType, typename Handler>
    void async_write_buffers(BufferType buffers, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            asio::async_write(*ssl_stream_, std::move(buffers), std::move(handler));
#endif
        } else {
            asio::async_write(socket_, std::move(buffers), std::move(handler));
        }
    }

    asio::io_context& ios_;
    asio::ip::tcp::socket socket_;

    std::string host_;
    unsigned short port_ = 0;
    asio::steady_timer reconnect_delay_timer_;
    size_t reconnect_delay_ms       = 1000; // s
    int reconnect_cnt_              = -1;
    std::atomic_bool has_connected_ = { false };
    std::mutex conn_mtx_;
    std::condition_variable conn_cond_;
    bool has_wait_ = false;

    asio::steady_timer deadline_;
    bool stop_client_ = false;

    struct client_message_type {
        std::uint64_t req_id;
        request_type req_type;
        rpc_service::rpc_buffer_type content;
        uint32_t func_id;
    };
    std::deque<client_message_type> outbox_;
    uint32_t write_size_ = 0;
    std::mutex write_mtx_;
    uint64_t fu_id_ = 0;
    std::function<void(asio::error_code)> err_cb_;
    bool enable_reconnect_ = false;

    std::unordered_map<std::uint64_t, std::shared_ptr<std::promise<req_result>>> future_map_;
    std::unordered_map<std::uint64_t, std::shared_ptr<call_t>> callback_map_;
    std::mutex cb_mtx_;
    uint64_t callback_id_ = 0;

    uint64_t temp_req_id_ = 0;

    char head_[kRpcHeadLen] = {};
    std::vector<char> body_;

    std::uint8_t write_head_[kRpcHeadLen] = {};

    std::mutex sub_mtx_;
    std::unordered_map<std::string, std::function<void(string_view)>> sub_map_;
    //
    std::shared_ptr<RpcClientProxyInterface> proxy_interface;
    rpc_client_ssl_level ssl_level = rpc_client_ssl_level::rpc_client_ssl_level_none;
#ifndef RPC_DISABLE_SSL
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket&>> ssl_stream_ = nullptr;
    std::string pem_path;
#endif
    // rpc handler
    std::atomic_bool has_upgrade = false;
};

inline ClientStreamReadWriter::ClientStreamReadWriter(rpc_client& client) : client_(client) {
    read_head();
}
inline ClientStreamReadWriter::~ClientStreamReadWriter() {
    if (last_data_status_ < rpc_stream_data_status::rpc_stream_finish) {
        set_status(rpc_stream_data_status::rpc_stream_finish,
                   error::make_error_code(error::rpc_errors::rpc_internal_error));
    }
    client_.close();
}
template <typename T>
inline bool ClientStreamReadWriter::Read(T& request, std::size_t max_wait_ms) {
    auto check_func = [this]() -> bool {
        return last_data_status_ >= rpc_stream_data_status::rpc_stream_write_done || !response_cache_.empty();
    };
    std::vector<std::uint8_t> response_data;
    {
        std::unique_lock<std::mutex> locker(mutex);
        if (max_wait_ms > 0)
            cv_.wait_for(locker, std::chrono::milliseconds(max_wait_ms), check_func);
        else {
            cv_.wait(locker, check_func);
        }
        if (response_cache_.empty() || last_data_status_ == rpc_stream_data_status::rpc_stream_failed) return false;
        response_data = std::move(response_cache_.front());
        response_cache_.pop_front();
    }
    try {
        request = msgpack_codec::unpack<T>(response_data.data(), response_data.size());
    } catch (const std::exception& e) {
        set_status(rpc_stream_data_status::rpc_stream_failed,
                   error::make_error_code(error::rpc_errors::rpc_unpack_failed));

        return false;
    }

    return true;
}
template <typename U>
inline bool ClientStreamReadWriter::Write(U&& response) {
    if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return false;
    rpc_buffer_type data;
    try {
        data = msgpack_codec::pack(std::forward<U>(response));
    } catch (const std::exception& e) {
        set_status(rpc_stream_data_status::rpc_stream_failed,
                   error::make_error_code(error::rpc_errors::rpc_pack_failed));

        return false;
    }
    asio::post(client_.socket_.get_executor(), [this, data = std::move(data)]() mutable {
        auto& new_item = write_cache_.emplace_back();
        new_item.size  = htole32(static_cast<std::uint32_t>(data.size()));
        new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_data);
        new_item.data  = std::move(data);
        if (write_cache_.size() == 1) handle_write();
    });
    return true;
}

inline bool ClientStreamReadWriter::WriteDone() {
    if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return false;
    asio::post(client_.socket_.get_executor(), [this]() mutable {
        auto& new_item = write_cache_.emplace_back();
        new_item.size  = 0;
        new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_write_done);
        new_item.data.clear();
        if (write_cache_.size() == 1) handle_write();
    });
    return true;
}

inline std::error_code ClientStreamReadWriter::Finish(std::size_t max_wait_ms) {
    do {
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) break;
        asio::post(client_.socket_.get_executor(), [this]() mutable {
            auto& new_item = write_cache_.emplace_back();
            new_item.size  = 0;
            new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_finish);
            new_item.data.clear();
            if (write_cache_.size() == 1) handle_write();
        });
        auto check_func = [this]() -> bool { return last_data_status_ >= rpc_stream_data_status::rpc_stream_finish; };
        std::unique_lock<std::mutex> locker(mutex);
        if (max_wait_ms > 0)
            cv_.wait_for(locker, std::chrono::milliseconds(max_wait_ms), check_func);
        else {
            cv_.wait(locker, check_func);
        }
    } while (0);

    return last_err_;
}

inline std::error_code ClientStreamReadWriter::GetLastError() const {
    return last_err_;
}
inline void ClientStreamReadWriter::read_head() {
    client_.async_buffer_read(
        { asio::buffer(&read_packet_buffer.size, sizeof(read_packet_buffer.size)) },
        [this](asio::error_code ec, std::size_t length) {
            if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
            if (ec) {
                set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
            } else {
#ifdef RPC_VERBOSE
                FDEBUG("client stream read head : {}",
                       Fundamental::Utils::BufferToHex(&read_packet_buffer.size, sizeof(read_packet_buffer.size)));
#endif
                read_packet_buffer.size = le32toh(read_packet_buffer.size);
                try {
                    read_packet_buffer.data.resize(read_packet_buffer.size);
                    read_body();
                } catch (...) {
                    set_status(rpc_stream_data_status::rpc_stream_failed,
                               error::make_error_code(error::rpc_errors::rpc_memory_error));
                }
            }
        });
}
inline void ClientStreamReadWriter::read_body() {
    std::vector<asio::mutable_buffer> buffers;
    buffers.emplace_back(asio::buffer(&read_packet_buffer.type, 1));
    if (read_packet_buffer.size > 0)
        buffers.emplace_back(asio::buffer(read_packet_buffer.data.data(), read_packet_buffer.size));
    client_.async_buffer_read(std::move(buffers), [this](asio::error_code ec, std::size_t length) {
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_write_done) return;
        if (ec) {
            set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
        } else {
#ifdef RPC_VERBOSE
            FDEBUG("client stream read :{:x} {}", read_packet_buffer.type,
                   Fundamental::Utils::BufferToHex(read_packet_buffer.data.data(), read_packet_buffer.size,140));
#endif
            auto status = static_cast<rpc_stream_data_status>(read_packet_buffer.type);
            if (status < last_data_status_ || status >= rpc_stream_data_status::rpc_stream_finish) {
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_bad_request));
                return;
            }
            switch (status) {
            case rpc_stream_data_status::rpc_stream_data: {
                read_head();
                std::scoped_lock<std::mutex> locker(mutex);
                last_data_status_ = rpc_stream_data_status::rpc_stream_data;
                response_cache_.emplace_back(std::move(read_packet_buffer.data));
                cv_.notify_one();
            } break;
            case rpc_stream_data_status::rpc_stream_write_done: {
                set_status(status, error::make_error_code(error::rpc_errors::rpc_success));
                read_head();
            } break;
            default: {
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_bad_request));
                break;
            }
            }
        }
    });
}
inline void ClientStreamReadWriter::set_status(rpc_stream_data_status status, std::error_code ec) {
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        cv_.notify_all();
        return;
    }
    std::scoped_lock<std::mutex> locker(mutex);
    last_err_         = std::move(ec);
    last_data_status_ = status;
    cv_.notify_all();
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        client_.close();
    }
}
inline void ClientStreamReadWriter::handle_write() {
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) write_cache_.clear();
    if (write_cache_.empty()) {
        return;
    }

    std::vector<asio::const_buffer> write_buffers;
    auto& item = write_cache_.front();
    write_buffers.emplace_back(asio::const_buffer(&item.size, sizeof(item.size)));
    write_buffers.emplace_back(asio::const_buffer(&item.type, sizeof(item.type)));
    if (item.data.size() > 0) {
        write_buffers.emplace_back(asio::const_buffer(item.data.data(), item.data.size()));
    }

    client_.async_write_buffers(std::move(write_buffers), [this](asio::error_code ec, std::size_t length) {
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
        if (ec) {
            set_status(rpc_stream_data_status::rpc_stream_failed, ec);
            return;
        }
#ifdef RPC_VERBOSE
        auto& item = write_cache_.front();
        FDEBUG("client stream write {}{}{}", Fundamental::Utils::BufferToHex(&item.size, sizeof(item.size)),
               Fundamental::Utils::BufferToHex(&item.type, sizeof(item.type)),
               Fundamental::Utils::BufferToHex(item.data.data(), item.data.size(),140));
#endif
        auto packet_type = write_cache_.front().type;
        if (packet_type == static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_finish)) {
            write_cache_.clear();
            set_status(rpc_stream_data_status::rpc_stream_finish,
                       error::make_error_code(error::rpc_errors::rpc_success));
        } else {
            write_cache_.pop_front();
            handle_write();
        }
    });
}
} // namespace rpc_service
} // namespace network
