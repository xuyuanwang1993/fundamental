#ifndef REST_RPC_CONNECTION_H_
#define REST_RPC_CONNECTION_H_

#include "basic/const_vars.h"
#include "basic/router.hpp"
#include "basic/use_asio.hpp"
#include "proxy/proxy_codec.hpp"
#include "proxy/proxy_handler.hpp"
#include "proxy/proxy_manager.hpp"

#include <any>
#include <array>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <unordered_set>

#include "fundamental/basic/log.h"
#include "fundamental/events/event_system.h"

using asio::ip::tcp;

namespace network
{
namespace rpc_service
{
struct rpc_server_ssl_config {
    std::function<std::string(std::string)> passwd_cb;
    std::string certificate_path;
    std::string private_key_path;
    std::string tmp_dh_path;
};

class rpc_server;
// call these interface not in io thread
class ServerStreamReadWriter : public std::enable_shared_from_this<ServerStreamReadWriter> {
    friend class connection;

public:
    Fundamental::Signal<void()> notify_stream_abort;
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<ServerStreamReadWriter>(std::forward<Args>(args)...);
    }
    ~ServerStreamReadWriter();
    template <typename T>
    bool Read(T& request, std::size_t max_wait_ms = 5000);
    template <typename U>
    bool Write(U&& response);
    bool WriteDone();
    std::error_code Finish(std::size_t max_wait_ms = 5000);
    std::error_code GetLastError() const;
    void EnableTimeoutCheck(std::size_t timeout_msec = 15000);
    ServerStreamReadWriter(std::shared_ptr<connection> conn);
    void release_obj();

private:
    void start() {
        read_head();
    }
    void reponse_heartbeat();
    void read_head();
    void read_body(std::uint32_t offset = 0);
    void set_status(rpc_stream_data_status status, std::error_code ec);
    void handle_write();
    void reset_timer() {
        if (timeout_msec_ == 0) {
            return;
        }
        timeout_check_timer_.expires_after(std::chrono::milliseconds(timeout_msec_));
        timeout_check_timer_.async_wait([this, ptr = shared_from_this()](const asio::error_code& ec) {
            if (!reference_.is_valid()) {
                return;
            }
            if (ec) {
                return;
            }
            if (b_waiting_process_any_data) {
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_timeout));
            } else {
                b_waiting_process_any_data.exchange(true);
                reset_timer();
            }
        });
    }

    void cancel_timer() {
        if (timeout_msec_ == 0) {
            return;
        }
        try {
            timeout_check_timer_.cancel();
        } catch (...) {
        }
    }

private:
    network_data_reference reference_;
    std::mutex mutex;
    std::error_code last_err_;
    rpc_stream_packet read_packet_buffer;
    std::atomic<rpc_stream_data_status> last_data_status_ = rpc_stream_data_status::rpc_stream_none;

    std::shared_ptr<connection> conn_;
    std::condition_variable cv_;
    std::deque<std::vector<std::uint8_t>> request_cache_;
    std::deque<rpc_stream_packet> write_cache_;
    asio::steady_timer timeout_check_timer_;
    std::atomic_bool b_waiting_process_any_data = false;
    std::size_t timeout_msec_                   = 0;
    std::list<asio::const_buffer> write_buffers_;
};

class connection : public std::enable_shared_from_this<connection>, private asio::noncopyable {
    friend class rpc_server;
    friend class ServerStreamReadWriter;

public:
    Fundamental::Signal<void(std::string, std::weak_ptr<connection>)> on_new_subscriber_added;
    Fundamental::Signal<void(const std::unordered_set<std::string>&, std::weak_ptr<connection>)> on_subscribers_removed;
    Fundamental::Signal<void(std::shared_ptr<connection>, std::string)> on_net_err_;
    Fundamental::Signal<void(std::string /*key*/, std::string /*data*/)> on_publish_msg;

public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<connection>(std::forward<Args>(args)...);
    }
    connection(tcp::socket socket, std::size_t timeout_msec, router& router, std::weak_ptr<rpc_server> server_wref) :
    server_wref_(server_wref), socket_(std::move(socket)), body_(INIT_BUF_SIZE),
    timeout_check_timer_(socket_.get_executor()), timeout_msec_(timeout_msec), router_(router) {
    }
    ~connection() {
        FDEBUG("release connection {:p} -> {}", (void*)this, conn_id_);
    }

    void start() {
        if (is_ssl()) {
            ssl_handshake();
        } else {
            read_head();
        }
    }

    tcp::socket& socket() {
        return socket_;
    }

    uint64_t request_id() const {
        return req_id_;
    }
    template <typename... Args>
    void response(uint64_t req_id, request_type req_type, Args&&... args) {
        auto data   = msgpack_codec::pack(static_cast<int32_t>(result_code::OK), std::forward<Args>(args)...);
        auto s_data = std::string(data.data(), data.data() + data.size());
        asio::post(socket_.get_executor(),
                   [this, data = std::move(s_data), req_id, req_type, ref = shared_from_this()]() mutable {
                       if (!reference_.is_valid()) {
                           return;
                       }
                       response_interal(req_id, std::move(data), req_type);
                   });
    }

    void set_conn_id(int64_t id) {
        conn_id_ = id;
    }

    int64_t conn_id() const {
        return conn_id_;
    }

    const std::vector<char>& body() const {
        return body_;
    }

    std::string remote_address() const {
        if (!reference_.is_valid()) {
            return "";
        }

        asio::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (ec) {
            return "";
        }
        return endpoint.address().to_string();
    }

    void set_delay(bool delay) {
        delay_ = delay;
    }

#ifndef RPC_DISABLE_SSL
    void enable_ssl(asio::ssl::context& ssl_context) {
        ssl_context_ref = &ssl_context;
    }
#endif
    void release_obj() {
        reference_.release();
        asio::post(socket_.get_executor(), [this, ref = shared_from_this()] { close(); });
    }
    std::shared_ptr<ServerStreamReadWriter> InitRpcStream();

    void config_proxy_manager(network::proxy::ProxyManager* manager) {
        proxy_manager_ = manager;
    }
    void set_tcp_no_delay(bool flag = true) {
        try {
            socket_.set_option(asio::ip::tcp::no_delay(flag));
        } catch (...) {
        }
    }
    bool has_closed() const {
        return !reference_.is_valid();
    }

private:
    void publish(const std::string& key, const std::string& data) {
        response(0, request_type::rpc_publish, key, data);
    }

    void do_ssl_handshake(const char* preread_data, std::size_t read_len) {
        // handle ssl
#ifndef RPC_DISABLE_SSL
        auto self   = this->shared_from_this();
        ssl_stream_ = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket&>>(socket_, *ssl_context_ref);

        ssl_stream_->async_handshake(asio::ssl::stream_base::server, asio::const_buffer(preread_data, read_len),
                                     [this, self](const asio::error_code& error, std::size_t) {
                                         if (!reference_.is_valid()) {
                                             return;
                                         }
                                         if (error) {
                                             FDEBUG("perform ssl handshake failed {}", error.message());
                                             on_net_err_(self, error.message());
                                             release_obj();
                                             return;
                                         }

                                         read_head();
                                     });
#endif
    }
    void ssl_handshake() {
        auto self(this->shared_from_this());
        asio::async_read(socket_, asio::buffer(head_, kSslPreReadSize),
                         [this, self](asio::error_code ec, std::size_t length) {
                             if (!reference_.is_valid()) {
                                 return;
                             }
                             if (!socket_.is_open()) {
                                 on_net_err_(self, "socket already closed");
                                 return;
                             }
                             do {
                                 if (ec) {
                                     FDEBUG("ssl handshake read failed {}", ec.message());
                                     on_net_err_(self, ec.message());

                                     break;
                                 }
                                 const std::uint8_t* p_data = (const std::uint8_t*)head_;
                                 if (p_data[0] == 0x16) // ssl Handshake
                                 {
                                     if (p_data[1] != 0x03 || p_data[2] > 0x03) {
                                         on_net_err_(self, "rpc only support SSL 3.0 to TLS 1.2");
                                         break;
                                     }
                                     do_ssl_handshake(head_, kSslPreReadSize);
                                 } else {
                                     FDEBUG("[rpc] WARNNING!!! falling  down to no ssl socket");
                                     ssl_context_ref = nullptr;
                                     read_head(kSslPreReadSize);
                                 }
                                 return;
                             } while (0);
                             release_obj();
                         });
    }

    bool is_ssl() const {
#ifndef RPC_DISABLE_SSL
        return ssl_context_ref != nullptr;
#else
        return false;
#endif
    }
    void process_rpc_request() {
        rpc_header header;
        header.DeSerialize(head_, kRpcHeadLen);
        req_id_                 = header.req_id;
        const uint32_t body_len = header.body_len;
        req_type_               = header.req_type;
        switch (req_type_) {
        case request_type::rpc_req:
        case request_type::rpc_subscribe:
        case request_type::rpc_unsubscribe:
        case request_type::rpc_publish: {
            if (body_len > 0) {
                if (body_len >= MAX_BUF_LEN) {
                    response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "body too large"));
                    break;
                }
                if (body_.size() < body_len) {
                    body_.resize(body_len);
                }
                read_body(header.func_id, body_len);
            } else if (req_type_ == request_type::rpc_req) {
                handle_none_param_request(header.func_id);
            } else {
                response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request"));
            }
        } break;
        case request_type::rpc_stream: {
            process_rpc_stream(header.func_id);
        } break;
        case request_type::rpc_heartbeat: {
            read_head();
            response_interal(req_id_, "", network::rpc_service::request_type::rpc_heartbeat);
            return;
        }
        default: {
            response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request type"));
            break;
        }
        }
    }
    void process_proxy_request();
    void read_head(std::size_t offset = 0) {
        FASSERT(offset < kRpcHeadLen);
        auto self(this->shared_from_this());
        async_buffer_read({ asio::buffer(head_ + offset, kRpcHeadLen - offset) }, [this, self](asio::error_code ec,
                                                                                               std::size_t length) {
            if (!reference_.is_valid()) {
                return;
            }
            if (!socket_.is_open()) {
                on_net_err_(self, "socket already closed");
                return;
            }
            if (ec) {
                FDEBUG("{:p} rpc read head failed {}", (void*)this, ec.message());
                on_net_err_(self, ec.message());
                release_obj();
                return;
            }
            b_waiting_process_any_data.exchange(false);
#ifdef RPC_VERBOSE
            FDEBUG("server {:p} read head {}", (void*)this, Fundamental::Utils::BufferToHex(head_, kRpcHeadLen));
#endif
            switch (head_[0]) {
            case RPC_MAGIC_NUM: process_rpc_request(); break;
            case network::proxy::ProxyRequest::kMagicNum: process_proxy_request(); break;
            default: {
                FERR("{:p} protocol error magic  {:02x}", (void*)this, static_cast<std::uint8_t>(head_[0]));
                release_obj();
            } break;
            }
        });
    }

    void read_body(uint32_t func_id, std::size_t size, std::size_t start_offset = 0) {
        auto self(this->shared_from_this());
#ifdef RPC_VERBOSE
        FDEBUG("server {:p} try read size: {}", (void*)this, size - start_offset);
#endif
        async_buffer_read_some(
            { asio::mutable_buffer(body_.data() + start_offset, size - start_offset) },
            [this, func_id, self, size, start_offset](asio::error_code ec, std::size_t length) {
                if (!reference_.is_valid()) {
                    return;
                }
                if (!socket_.is_open()) {
                    on_net_err_(self, "socket already closed");
                    return;
                }
                if (ec) {
                    // do't close socket, wait for write done
                    on_net_err_(self, ec.message());
                    return;
                }
                b_waiting_process_any_data.exchange(false);
                auto current_read_offset = start_offset + length;
#ifdef RPC_VERBOSE
                FDEBUG("server {:p} read some need:{}  current: {} new:{}", (void*)this, size, current_read_offset,
                       length);
#endif
                if (current_read_offset < size) {
                    read_body(func_id, size, current_read_offset);
                    return;
                }
#ifdef RPC_VERBOSE
                FDEBUG("server {:p} read {} body {}", (void*)this, size,
                       Fundamental::Utils::BufferToHex(body_.data(), size, 140));
#endif
                read_head();
                try {
                    switch (req_type_) {
                    case request_type::rpc_req: process_request(func_id, body_.data(), size); break;
                    case request_type::rpc_subscribe: {
                        msgpack_codec codec;
                        auto p              = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), size);
                        auto new_subscriber = std::get<0>(p);
                        auto ret            = subscribers_.insert(new_subscriber);
                        if (ret.second) {
                            on_new_subscriber_added(new_subscriber, weak_from_this());
                        }
                        response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                    } break;
                    case request_type::rpc_unsubscribe: {
                        msgpack_codec codec;
                        auto p                  = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), size);
                        auto removed_subscriber = std::get<0>(p);
                        auto iter               = subscribers_.find(removed_subscriber);
                        if (iter != subscribers_.end()) {
                            subscribers_.erase(iter);
                            on_subscribers_removed({ removed_subscriber }, weak_from_this());
                        }
                        response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                    } break;
                    case request_type::rpc_publish: {
                        msgpack_codec codec;
                        auto p    = codec.unpack_tuple<std::tuple<std::string, std::string>>(body_.data(), size);
                        auto& key = std::get<0>(p);
                        auto& msg = std::get<1>(p);
                        on_publish_msg(std::move(key), std::move(msg));
                        response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                    } break;
                    default: {
                        auto result = msgpack_codec::pack_args_str(
                            result_code::FAIL,
                            Fundamental::StringFormat("bad request type:{}", static_cast<std::int32_t>(req_type_)));
                        response_interal(req_id_, std::move(result));
                    } break;
                    }
                } catch (const std::exception& ex) {
                    auto result = msgpack_codec::pack_args_str(result_code::FAIL, ex.what());
                    response_interal(req_id_, std::move(result));
                }
            });
    }
    void handle_none_param_request(uint32_t func_id) {
        read_head();
        process_request(func_id, nullptr, 0);
    }
    void process_request(uint32_t func_id, const char* data, std::size_t size) {
        route_result_t ret =
            router_.route<connection>(func_id, std::string_view { data, size }, this->shared_from_this());
        if (delay_) {
            delay_ = false;
        } else {
            response_interal(req_id_, std::move(ret.result));
        }
    }

    void process_rpc_stream(uint32_t func_id) {
        cancel_timer();
        // allocate rpc stream
        rpc_stream_rw_ = ServerStreamReadWriter::make_shared(this->shared_from_this());
        route_result_t ret =
            router_.route<connection>(func_id, std::string_view { nullptr, 0 }, this->shared_from_this());
        // force clear rpc stream cache
        rpc_stream_rw_ = nullptr;
        response_interal(req_id_, std::move(ret.result), request_type::rpc_stream);
    }

    void response_interal(uint64_t req_id, std::string data, request_type req_type = request_type::rpc_res) {
        assert(data.size() < MAX_BUF_LEN);

        write_queue_.emplace_back(message_type { req_id, req_type, std::move(data) });
        if (write_queue_.size() > 1) {
            return;
        }

        write();
    }

    void write() {
        if (write_buffers_.empty()) {
            auto& msg   = write_queue_.front();
            write_size_ = (uint32_t)msg.content.size();
            rpc_header { RPC_MAGIC_NUM, msg.req_type, write_size_, msg.req_id, static_cast<std::uint32_t>(conn_id_) }
                .Serialize(write_head_buffer, kRpcHeadLen);

            write_buffers_.emplace_back(asio::buffer(write_head_buffer, kRpcHeadLen));
            if (write_size_ > 0) write_buffers_.emplace_back(asio::buffer(msg.content.data(), write_size_));
        }

        auto self = this->shared_from_this();
        async_write_buffers_some(std::vector<asio::const_buffer>(write_buffers_.begin(), write_buffers_.end()),
                                 [this, self](asio::error_code ec, std::size_t length) {
                                     if (!reference_.is_valid()) {
                                         return;
                                     }
                                     if (ec) {
                                         FDEBUG("{:p} rpc write failed {}", (void*)this, ec.message());
                                         on_net_err_(shared_from_this(), ec.message());
                                         release_obj();
                                         return;
                                     }
                                     b_waiting_process_any_data.exchange(false);
#ifdef RPC_VERBOSE
                                     FDEBUG("server {:p} write some size:{}", (void*)this, length);
#endif
                                     while (length != 0) {
                                         if (write_buffers_.empty()) break;
                                         auto current_size = write_buffers_.front().size();
                                         if (length >= current_size) {
                                             length -= current_size;
                                             write_buffers_.pop_front();
                                             continue;
                                         }
                                         write_buffers_.front() =
                                             asio::const_buffer((std::uint8_t*)write_buffers_.front().data() + length,
                                                                current_size - length);
                                         break;
                                     }
                                     if (!write_buffers_.empty()) { // write rest data

                                         write();
                                         return;
                                     }
                                     write_queue_.pop_front();

                                     if (!write_queue_.empty()) {
                                         write();
                                     }
                                 });
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

    template <typename Handler>
    void async_buffer_read_some(std::vector<asio::mutable_buffer> buffers, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            ssl_stream_->async_read_some(std::move(buffers), std::move(handler));
#endif
        } else {
            socket_.async_read_some(std::move(buffers), std::move(handler));
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
    template <typename BufferType, typename Handler>
    void async_write_buffers_some(BufferType&& buffers, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            ssl_stream_->async_write_some(std::move(buffers), std::move(handler));
#endif
        } else {
            socket_.async_write_some(std::move(buffers), std::move(handler));
        }
    }
    void reset_timer() {
        if (timeout_msec_ == 0) {
            return;
        }

        auto self(this->shared_from_this());
        timeout_check_timer_.expires_after(std::chrono::milliseconds(timeout_msec_));
        timeout_check_timer_.async_wait([this, self](const asio::error_code& ec) {
            if (!reference_.is_valid()) {
                return;
            }

            if (ec) {
                return;
            }
            if (b_waiting_process_any_data) {
                release_obj();
            } else {
                b_waiting_process_any_data.exchange(true);
                reset_timer();
            }
        });
    }

    void cancel_timer() {
        if (timeout_msec_ == 0) {
            return;
        }
        try {
            timeout_check_timer_.cancel();
        } catch (...) {
        }
    }

    void close() {
        {
            std::unordered_set<std::string> tmp;
            std::swap(subscribers_, tmp);
            on_subscribers_removed(tmp, weak_from_this());
        }

#ifndef RPC_DISABLE_SSL
        if (ssl_stream_) {
            asio::error_code ec;
            ssl_stream_->shutdown(ec);
            ssl_stream_->lowest_layer().cancel(ec);
            ssl_stream_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        }
#endif
        cancel_timer();
        asio::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
    }
    network_data_reference reference_;
    std::weak_ptr<rpc_server> server_wref_;
    tcp::socket socket_;
    char head_[kRpcHeadLen];
    std::vector<char> body_;
    std::uint64_t req_id_;
    request_type req_type_;

    std::uint8_t write_head_buffer[kRpcHeadLen];

    uint32_t write_size_ = 0;

    std::atomic_bool b_waiting_process_any_data = false;
    asio::steady_timer timeout_check_timer_;
    std::size_t timeout_msec_;
    int64_t conn_id_ = 0;
    std::unordered_set<std::string> subscribers_;

    std::deque<message_type> write_queue_;
    std::list<asio::const_buffer> write_buffers_;
    std::shared_ptr<ServerStreamReadWriter> rpc_stream_rw_;

    router& router_;
    bool delay_ = false;
#ifndef RPC_DISABLE_SSL
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket&>> ssl_stream_ = nullptr;
    asio::ssl::context* ssl_context_ref                                    = nullptr;
#endif
    // proxy
    network::proxy::ProxyManager* proxy_manager_ = nullptr;
};

inline ServerStreamReadWriter::ServerStreamReadWriter(std::shared_ptr<connection> conn) :
conn_(conn), timeout_check_timer_(conn_->socket_.get_executor()) {
    FDEBUG("build stream writer {:p} with connection:{:p}", (void*)this, (void*)conn_.get());
}
inline void ServerStreamReadWriter::release_obj() {
    reference_.release();
    asio::post(conn_->socket_.get_executor(), [this, ref = shared_from_this()] {
        cancel_timer();
        if (last_data_status_ < rpc_stream_data_status::rpc_stream_finish) {
            set_status(rpc_stream_data_status::rpc_stream_finish,
                       error::make_error_code(error::rpc_errors::rpc_internal_error));
        }
        conn_->release_obj();
    });
}
inline ServerStreamReadWriter::~ServerStreamReadWriter() {
    FDEBUG("release stream writer {:p} with connection:{:p}", (void*)this, (void*)conn_.get());
}
template <typename T>
inline bool ServerStreamReadWriter::Read(T& request, std::size_t max_wait_ms) {
    auto check_func = [this]() -> bool {
        return last_data_status_ >= rpc_stream_data_status::rpc_stream_write_done || !request_cache_.empty();
    };
    std::vector<std::uint8_t> request_data;
    {
        std::unique_lock<std::mutex> locker(mutex);
        if (max_wait_ms > 0)
            cv_.wait_for(locker, std::chrono::milliseconds(max_wait_ms), check_func);
        else {
            cv_.wait(locker, check_func);
        }
        if (request_cache_.empty() || last_data_status_ == rpc_stream_data_status::rpc_stream_failed) return false;
        request_data = std::move(request_cache_.front());
        request_cache_.pop_front();
    }
    try {
        request = msgpack_codec::unpack<T>(request_data.data(), request_data.size());
    } catch (const std::exception& e) {
        set_status(rpc_stream_data_status::rpc_stream_failed,
                   error::make_error_code(error::rpc_errors::rpc_unpack_failed));

        return false;
    }

    return true;
}
template <typename U>
inline bool ServerStreamReadWriter::Write(U&& response) {
    if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return false;
    rpc_buffer_type data;
    try {
        data = msgpack_codec::pack(std::forward<U>(response));
    } catch (const std::exception& e) {
        set_status(rpc_stream_data_status::rpc_stream_failed,
                   error::make_error_code(error::rpc_errors::rpc_pack_failed));

        return false;
    }
    asio::post(conn_->socket_.get_executor(), [this, data = std::move(data), ref = shared_from_this()]() mutable {
        if (!reference_.is_valid()) return;
        auto& new_item = write_cache_.emplace_back();
        new_item.size  = htole32(static_cast<std::uint32_t>(data.size()));
        new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_data);
        new_item.data  = std::move(data);
        if (write_cache_.size() == 1) handle_write();
    });
    return true;
}

inline bool ServerStreamReadWriter::WriteDone() {
    if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return false;
    asio::post(conn_->socket_.get_executor(), [this, ref = shared_from_this()]() mutable {
        if (!reference_.is_valid()) return;
        auto& new_item = write_cache_.emplace_back();
        new_item.size  = 0;
        new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_write_done);
        new_item.data.clear();
        if (write_cache_.size() == 1) handle_write();
    });
    return true;
}

inline std::error_code ServerStreamReadWriter::Finish(std::size_t max_wait_ms) {
    auto check_func = [this]() -> bool { return last_data_status_ >= rpc_stream_data_status::rpc_stream_finish; };
    std::unique_lock<std::mutex> locker(mutex);
    if (max_wait_ms > 0)
        cv_.wait_for(locker, std::chrono::milliseconds(max_wait_ms), check_func);
    else {
        cv_.wait(locker, check_func);
    }
    return last_err_;
}

inline std::error_code ServerStreamReadWriter::GetLastError() const {
    return last_err_;
}
inline void ServerStreamReadWriter::EnableTimeoutCheck(std::size_t timeout_msec) {
    timeout_msec_ = timeout_msec;
    reset_timer();
}
inline void ServerStreamReadWriter::reponse_heartbeat() {
    auto& new_item = write_cache_.emplace_back();
    new_item.size  = 0;
    new_item.type  = static_cast<std::uint8_t>(rpc_stream_data_status::rpc_stream_heartbeat);
    new_item.data.clear();
    if (write_cache_.size() == 1) handle_write();
}
inline void ServerStreamReadWriter::read_head() {
    std::vector<asio::mutable_buffer> buffers;
    buffers.emplace_back(&read_packet_buffer.size, sizeof(read_packet_buffer.size));
    buffers.emplace_back(asio::buffer(&read_packet_buffer.type, 1));

    conn_->async_buffer_read(std::move(buffers), [this, ptr = shared_from_this()](asio::error_code ec,
                                                                                  std::size_t length) {
        if (!reference_.is_valid()) {
            return;
        }
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
        if (ec) {
            set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
            notify_stream_abort.Emit();
        } else {
#ifdef RPC_VERBOSE
            FDEBUG("server {:p} stream read head size data:{} type:{}", (void*)this,
                   Fundamental::Utils::BufferToHex(&read_packet_buffer.size, sizeof(read_packet_buffer.size)),
                   static_cast<std::uint32_t>(read_packet_buffer.type));
#endif
            b_waiting_process_any_data.exchange(false);

            try {
                auto status = static_cast<rpc_stream_data_status>(read_packet_buffer.type);
                if ((status != rpc_stream_data_status::rpc_stream_heartbeat && status < last_data_status_) ||
                    status >= rpc_stream_data_status::rpc_stream_status_max) {
                    FDEBUG("server {:p} rpc_bad_request status:{} last_data_status:{}", (void*)this,
                           static_cast<std::uint32_t>(status), static_cast<std::uint32_t>(last_data_status_.load()));
                    set_status(rpc_stream_data_status::rpc_stream_failed,
                               error::make_error_code(error::rpc_errors::rpc_bad_request));
                    return;
                }
                switch (status) {
                case rpc_stream_data_status::rpc_stream_data: {
                    {
                        std::scoped_lock<std::mutex> locker(mutex);
                        last_data_status_ = status;
                    }
                    read_packet_buffer.size = le32toh(read_packet_buffer.size);
                    if (read_packet_buffer.size > read_packet_buffer.data.size())
                        read_packet_buffer.data.resize(read_packet_buffer.size);
                    read_body();

                } break;
                case rpc_stream_data_status::rpc_stream_heartbeat: {

                    reponse_heartbeat();
                    read_head();
                } break;
                case rpc_stream_data_status::rpc_stream_write_done: {
                    read_head();
                    set_status(status, error::make_error_code(error::rpc_errors::rpc_success));
                } break;
                case rpc_stream_data_status::rpc_stream_finish: {
                    if (last_data_status_ != rpc_stream_data_status::rpc_stream_write_done) {
                        set_status(rpc_stream_data_status::rpc_stream_failed,
                                   error::make_error_code(error::rpc_errors::rpc_broken_pipe));
                        return;
                    }
                    set_status(status, error::make_error_code(error::rpc_errors::rpc_success));
                } break;
                default: {
                    FDEBUG("server {:p} rpc_bad_request   unsupported status:{} last_data_status:{}", (void*)this,
                           static_cast<std::uint32_t>(status), static_cast<std::uint32_t>(last_data_status_.load()));
                    set_status(rpc_stream_data_status::rpc_stream_failed,
                               error::make_error_code(error::rpc_errors::rpc_bad_request));
                    break;
                }
                }

            } catch (...) {
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_memory_error));
            }
        }
    });
}
inline void ServerStreamReadWriter::read_body(std::uint32_t offset) {
    std::vector<asio::mutable_buffer> buffers;
    buffers.emplace_back(asio::buffer(read_packet_buffer.data.data() + offset, read_packet_buffer.size - offset));
    conn_->async_buffer_read_some(
        std::move(buffers), [this, offset, ptr = shared_from_this()](asio::error_code ec, std::size_t length) {
            if (!reference_.is_valid()) {
                return;
            }
            if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
            if (ec) {
                set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
                notify_stream_abort.Emit();
            } else {
                b_waiting_process_any_data.exchange(false);
                auto current_read_offset = offset + length;
#ifdef RPC_VERBOSE
                FDEBUG("server {:p} stream read some need:{}  current: {} new:{}", (void*)this, read_packet_buffer.size,
                       current_read_offset, length);
#endif
                if (current_read_offset < read_packet_buffer.size) {

                    read_body(current_read_offset);
                    return;
                }
#ifdef RPC_VERBOSE
                FDEBUG("server {:p} stream read {}", (void*)this,
                       Fundamental::Utils::BufferToHex(read_packet_buffer.data.data(), read_packet_buffer.size, 140));
#endif
                read_head();
                std::scoped_lock<std::mutex> locker(mutex);
                request_cache_.emplace_back(std::move(read_packet_buffer.data));
                cv_.notify_one();
            }
        });
}
inline void ServerStreamReadWriter::set_status(rpc_stream_data_status status, std::error_code ec) {
    std::scoped_lock<std::mutex> locker(mutex);
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        cv_.notify_all();
        return;
    }
    last_err_         = std::move(ec);
    last_data_status_ = status;
    cv_.notify_all();
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        FDEBUG("rpc {:p} stream connection finish success:{} {}", (void*)this,
               last_data_status_.load() == rpc_stream_data_status::rpc_stream_finish, last_err_.message());
        release_obj();
    }
}
inline void ServerStreamReadWriter::handle_write() {
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        write_buffers_.clear();
        write_cache_.clear();
    }

    if (write_cache_.empty()) {
        return;
    }
    if (write_buffers_.empty()) {
        auto& item = write_cache_.front();
        write_buffers_.emplace_back(asio::const_buffer(&item.size, sizeof(item.size)));
        write_buffers_.emplace_back(asio::const_buffer(&item.type, sizeof(item.type)));
        if (item.data.size() > 0) {
            write_buffers_.emplace_back(asio::const_buffer(item.data.data(), item.data.size()));
        }
    }

    conn_->async_write_buffers_some(
        std::vector<asio::const_buffer>(write_buffers_.begin(), write_buffers_.end()),
        [this, ptr = shared_from_this()](asio::error_code ec, std::size_t length) {
            if (!reference_.is_valid()) {
                return;
            }
            if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
            if (ec) {
                set_status(rpc_stream_data_status::rpc_stream_failed, ec);
                notify_stream_abort.Emit();
                return;
            }
            b_waiting_process_any_data.exchange(false);

#ifdef RPC_VERBOSE
            FDEBUG("server {:p} stream write some size:{}", (void*)this, length);
#endif
            while (length != 0) {
                if (write_buffers_.empty()) break;
                auto current_size = write_buffers_.front().size();
                if (length >= current_size) {
                    length -= current_size;
                    write_buffers_.pop_front();
                    continue;
                }
                write_buffers_.front() =
                    asio::const_buffer((std::uint8_t*)write_buffers_.front().data() + length, current_size - length);
                break;
            }
            if (!write_buffers_.empty()) { // write rest data

                handle_write();
                return;
            }
            write_cache_.pop_front();
            handle_write();
        });
}
} // namespace rpc_service
} // namespace network

#endif // REST_RPC_CONNECTION_H_
