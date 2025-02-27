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

namespace network {
namespace rpc_service {
struct rpc_server_ssl_config {
    std::function<std::string(std::string)> passwd_cb;
    std::string certificate_path;
    std::string private_key_path;
    std::string tmp_dh_path;
};

class rpc_server;
// call these interface not in io thread
class ServerStreamReadWriter {
    friend class connection;

public:
    ServerStreamReadWriter(std::shared_ptr<connection> conn);
    ~ServerStreamReadWriter();
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

    std::shared_ptr<connection> conn_;
    std::condition_variable cv_;
    std::deque<std::vector<std::uint8_t>> request_cache_;
    std::deque<rpc_stream_packet> write_cache_;
};

class connection : public std::enable_shared_from_this<connection>, private asio::noncopyable {
    friend class rpc_server;
    friend class ServerStreamReadWriter;

public:
    Fundamental::Signal<void(std::string, std::weak_ptr<connection>)> on_new_subscriber_added;
    Fundamental::Signal<void(const std::unordered_set<std::string>&, std::weak_ptr<connection>)> on_subscribers_removed;
    Fundamental::Signal<void(std::shared_ptr<connection>, std::string)> on_net_err_;
    Fundamental::Signal<void()> on_connection_closed;
    Fundamental::Signal<void(std::string /*key*/, std::string /*data*/)> on_publish_msg;

public:
    connection(tcp::socket socket, std::size_t timeout_seconds, router& router) :
    socket_(std::move(socket)), body_(INIT_BUF_SIZE), timer_(socket_.get_executor()), timeout_seconds_(timeout_seconds),
    has_closed_(false), router_(router) {
    }

    ~connection() {
        close();
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

    bool has_closed() const {
        return has_closed_;
    }
    uint64_t request_id() const {
        return req_id_;
    }
    template <typename... Args>
    void response(uint64_t req_id, request_type req_type, Args&&... args) {
        auto data   = msgpack_codec::pack(static_cast<int32_t>(result_code::OK), std::forward<Args>(args)...);
        auto s_data = std::string(data.data(), data.data() + data.size());
        std::weak_ptr<connection> weak = shared_from_this();
        asio::post(socket_.get_executor(), [this, weak, data = std::move(s_data), req_id, req_type]() mutable {
            auto conn = weak.lock();
            if (conn) {
                response_interal(req_id, std::move(data), req_type);
            }
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
        if (has_closed_) {
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
    void abort() {
        close();
        cancel_timer();
    }
    std::shared_ptr<ServerStreamReadWriter> InitRpcStream() {
        FASSERT(rpc_stream_rw_ != nullptr && "it's not a stream rpc call");
        std::shared_ptr<ServerStreamReadWriter> ret = rpc_stream_rw_;
        rpc_stream_rw_                              = nullptr;
        return ret;
    }

    void config_proxy_manager(network::proxy::ProxyManager* manager) {
        proxy_manager_ = manager;
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
                                         if (error) {
                                             FDEBUG("perform ssl handshake failed {}", error.message());
                                             on_net_err_(self, error.message());
                                             close();
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
#ifdef RPC_DEBUG
                                     std::cout << "[rpc] WARNNING!!! falling to no ssl socket" << std::endl;
#endif
                                     ssl_context_ref = nullptr;
                                     read_head(kSslPreReadSize);
                                 }
                                 return;
                             } while (0);
                             close();
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
        rpc_header* header      = (rpc_header*)(head_);
        req_id_                 = header->req_id;
        const uint32_t body_len = header->body_len;
        req_type_               = header->req_type;
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
                read_body(header->func_id, body_len);
            } else if (req_type_ == request_type::rpc_req) {
                handle_none_param_request(header->func_id);
            } else {
                response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request"));
            }
        } break;
        case request_type::rpc_stream: {
            process_rpc_stream(header->func_id);
        } break;
        case request_type::rpc_heartbeat: {
            cancel_timer();
            read_head();
            return;
        }
        default: {
            response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request type"));
            break;
        }
        }
    }
    void process_proxy_request() {
        using network::proxy::ProxyRequest;
        do {
            if (!proxy_manager_) {
                FDEBUG("data proxy not enabled");
                break;
            }
            auto data_size = ProxyRequest::PeekSize(head_ + 1);
            if (data_size > ProxyRequest::kMaxPayloadLen) {
                FERR("invalid proxy request size {} overflow {}", data_size, ProxyRequest::kMaxPayloadLen);
                break;
            }
            data_size += ProxyRequest::kHeaderLen;
            auto proxy_buffer = std::make_shared<std::vector<std::uint8_t>>(data_size);
            std::memcpy(proxy_buffer->data(), head_, kRpcHeadLen);
            reset_timer();
            auto self(this->shared_from_this());
            auto p_data = proxy_buffer->data() + kRpcHeadLen;
            async_buffer_read(
                { asio::buffer(p_data, data_size - kRpcHeadLen) },
                [this, self, proxy_buffer = std::move(proxy_buffer)](asio::error_code ec, std::size_t length) {
                    if (!socket_.is_open()) {
                        on_net_err_(self, "socket already closed");
                        return;
                    }
                    if (ec) {
                        FDEBUG("proxy init read failed {}", ec.message());
                        on_net_err_(self, ec.message());
                        close();
                        return;
                    }
                    do {
                        ProxyRequest request;
                        if (!request.FromBuf(proxy_buffer->data(), proxy_buffer->size())) {
                            FERR("invalid proxy request data");
                            FDEBUG("{}", Fundamental::Utils::BufferToHex(proxy_buffer->data(), proxy_buffer->size()));
                            break;
                        }
                        network::proxy::ProxyHost hostInfo;
                        if (!proxy_manager_->GetProxyHostInfo(request.service_name_, request.token_, request.field_,
                                                              hostInfo)) {
                            FERR("get proxy host failed");
                            break;
                        }

                        FDEBUG("start proxy {} {} {} -> {}:{}", request.service_name_, request.token_, request.field_,
                               hostInfo.host, hostInfo.service);
                        network::proxy::proxy_handler::MakeShared(hostInfo.host, hostInfo.service, std::move(socket_))
                            ->SetUp();
                        cancel_timer();
                        return;
                    } while (0);
                    close();
                });
            return;
        } while (0);
        close();
    }
    void read_head(std::size_t offset = 0) {
        reset_timer();
        auto self(this->shared_from_this());
        async_buffer_read({ asio::buffer(head_ + offset, kRpcHeadLen - offset) }, [this, self](asio::error_code ec,
                                                                                               std::size_t length) {
            if (!socket_.is_open()) {
                on_net_err_(self, "socket already closed");
                return;
            }
            if (ec) {
                FDEBUG("rpc read head failed {}", ec.message());
                on_net_err_(self, ec.message());
                close();
                return;
            }
            switch (head_[0]) {
            case RPC_MAGIC_NUM: process_rpc_request(); break;
            case network::proxy::ProxyRequest::kMagicNum: process_proxy_request(); break;
            default: {
                FERR("{:p} protocol error magic  {:02x}", (void*)this, static_cast<std::uint8_t>(head_[0]));
                close();
            } break;
            }
        });
    }

    void read_body(uint32_t func_id, std::size_t size) {
        auto self(this->shared_from_this());
        async_read(size, [this, func_id, self](asio::error_code ec, std::size_t length) {
            cancel_timer();

            if (!socket_.is_open()) {
                on_net_err_(self, "socket already closed");
                return;
            }
            if (ec) {
                // do't close socket, wait for write done
                on_net_err_(self, ec.message());
                return;
            }
            read_head();
            try {
                switch (req_type_) {
                case request_type::rpc_req: process_request(func_id, body_.data(), length); break;
                case request_type::rpc_subscribe: {
                    msgpack_codec codec;
                    auto p              = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), length);
                    auto new_subscriber = std::get<0>(p);
                    auto ret            = subscribers_.insert(new_subscriber);
                    if (ret.second) {
                        on_new_subscriber_added(new_subscriber, weak_from_this());
                    }
                    response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                } break;
                case request_type::rpc_unsubscribe: {
                    msgpack_codec codec;
                    auto p                  = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), length);
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
                    auto p    = codec.unpack_tuple<std::tuple<std::string, std::string>>(body_.data(), length);
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
        timer_.cancel();
        // allocate rpc stream
        rpc_stream_rw_ = std::make_shared<ServerStreamReadWriter>(this->shared_from_this());
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
        auto& msg   = write_queue_.front();
        write_size_ = (uint32_t)msg.content.size();
        header_     = { RPC_MAGIC_NUM, msg.req_type, write_size_, msg.req_id };
        std::array<asio::const_buffer, 2> write_buffers;
        write_buffers[0] = asio::buffer(&header_, sizeof(rpc_header));
        write_buffers[1] = asio::buffer(msg.content.data(), write_size_);

        auto self = this->shared_from_this();
        async_write(write_buffers, [this, self](asio::error_code ec, std::size_t length) {
            if (ec) {
                FDEBUG("rpc write failed {}", ec.message());
                on_net_err_(shared_from_this(), ec.message());
                close();
                return;
            }

            if (has_closed()) {
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
    void async_read(size_t size_to_read, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            asio::async_read(*ssl_stream_, asio::buffer(body_.data(), size_to_read), std::move(handler));
#endif
        } else {
            asio::async_read(socket_, asio::buffer(body_.data(), size_to_read), std::move(handler));
        }
    }

    template <typename BufferType, typename Handler>
    void async_write(const BufferType& buffers, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            asio::async_write(*ssl_stream_, buffers, std::move(handler));
#endif
        } else {
            asio::async_write(socket_, buffers, std::move(handler));
        }
    }

    void reset_timer() {
        if (timeout_seconds_ == 0) {
            return;
        }

        auto self(this->shared_from_this());
        timer_.expires_after(std::chrono::seconds(timeout_seconds_));
        timer_.async_wait([this, self](const asio::error_code& ec) {
            if (has_closed()) {
                return;
            }

            if (ec) {
                return;
            }

            close();
        });
    }

    void cancel_timer() {
        if (timeout_seconds_ == 0) {
            return;
        }

        timer_.cancel();
    }

    void close() {
        bool expected = false;
        if (!has_closed_.compare_exchange_strong(expected, true)) {
            return;
        }
        {
            std::unordered_set<std::string> tmp;
            std::swap(subscribers_, tmp);
            on_subscribers_removed(tmp, weak_from_this());
            on_connection_closed();
        }

#ifndef RPC_DISABLE_SSL
        if (ssl_stream_) {
            asio::error_code ec;
            ssl_stream_->shutdown(ec);
            ssl_stream_ = nullptr;
        }
#endif
        timer_.cancel();
        asio::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
    }

    tcp::socket socket_;
    char head_[kRpcHeadLen];
    std::vector<char> body_;
    std::uint64_t req_id_;
    request_type req_type_;

    rpc_header header_;

    uint32_t write_size_ = 0;

    asio::steady_timer timer_;
    std::size_t timeout_seconds_;
    int64_t conn_id_ = 0;
    std::atomic_bool has_closed_;

    std::unordered_set<std::string> subscribers_;

    std::deque<message_type> write_queue_;
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

inline ServerStreamReadWriter::ServerStreamReadWriter(std::shared_ptr<connection> conn) : conn_(conn) {
    read_head();
}
inline ServerStreamReadWriter::~ServerStreamReadWriter() {
    if (last_data_status_ < rpc_stream_data_status::rpc_stream_finish) {
        set_status(rpc_stream_data_status::rpc_stream_finish,
                   error::make_error_code(error::rpc_errors::rpc_internal_error));
    }
    conn_->close();
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
    asio::post(conn_->socket_.get_executor(), [this, data = std::move(data)]() mutable {
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
    asio::post(conn_->socket_.get_executor(), [this]() mutable {
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
inline void ServerStreamReadWriter::read_head() {

    conn_->async_buffer_read({ asio::buffer(&read_packet_buffer.size, sizeof(read_packet_buffer.size)) },
                             [this](asio::error_code ec, std::size_t length) {
                                 if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
                                 if (ec) {
                                     set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
                                 } else {
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
inline void ServerStreamReadWriter::read_body() {
    std::vector<asio::mutable_buffer> buffers;
    buffers.emplace_back(asio::buffer(&read_packet_buffer.type, 1));
    if (read_packet_buffer.size > 0)
        buffers.emplace_back(asio::buffer(read_packet_buffer.data.data(), read_packet_buffer.size));
    conn_->async_buffer_read(std::move(buffers), [this](asio::error_code ec, std::size_t length) {
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
        if (ec) {
            set_status(rpc_stream_data_status::rpc_stream_failed, std::move(ec));
        } else {
            auto status = static_cast<rpc_stream_data_status>(read_packet_buffer.type);
            if (status < last_data_status_ || status >= rpc_stream_data_status::rpc_stream_status_max) {
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_bad_request));
                return;
            }
            switch (status) {
            case rpc_stream_data_status::rpc_stream_data: {
                read_head();
                std::scoped_lock<std::mutex> locker(mutex);
                last_data_status_ = status;
                request_cache_.emplace_back(std::move(read_packet_buffer.data));
                cv_.notify_one();
            } break;
            case rpc_stream_data_status::rpc_stream_write_done: {
                if (last_data_status_ == rpc_stream_data_status::rpc_stream_write_done) {
                    set_status(rpc_stream_data_status::rpc_stream_failed,
                               error::make_error_code(error::rpc_errors::rpc_bad_request));
                    return;
                }
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
                set_status(rpc_stream_data_status::rpc_stream_failed,
                           error::make_error_code(error::rpc_errors::rpc_bad_request));
                break;
            }
            }
        }
    });
}
inline void ServerStreamReadWriter::set_status(rpc_stream_data_status status, std::error_code ec) {
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        cv_.notify_all();
        return;
    }
    std::scoped_lock<std::mutex> locker(mutex);
    last_err_         = std::move(ec);
    last_data_status_ = status;
    cv_.notify_all();
    if (last_data_status_.load() >= rpc_stream_data_status::rpc_stream_finish) {
        conn_->close();
    }
}
inline void ServerStreamReadWriter::handle_write() {
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

    conn_->async_write(std::move(write_buffers), [this](asio::error_code ec, std::size_t length) {
        if (last_data_status_ >= rpc_stream_data_status::rpc_stream_finish) return;
        if (ec) {
            set_status(rpc_stream_data_status::rpc_stream_failed, ec);
            return;
        }
        write_cache_.pop_front();
        handle_write();
    });
}
} // namespace rpc_service
} // namespace network

#endif // REST_RPC_CONNECTION_H_
