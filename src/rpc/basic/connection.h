#ifndef REST_RPC_CONNECTION_H_
#define REST_RPC_CONNECTION_H_

#include "const_vars.h"
#include "router.hpp"
#include "use_asio.hpp"
#include <any>
#include <array>
#include <deque>
#include <iostream>
#include <memory>

using asio::ip::tcp;

namespace network {
namespace rpc_service {
struct rpc_server_ssl_config {
    std::function<std::string(std::string)> passwd_cb;
    std::string certificate_path;
    std::string private_key_path;
    std::string tmp_dh_path;
};

class connection : public std::enable_shared_from_this<connection>, private asio::noncopyable {
public:
    connection(asio::io_context& io_service, std::size_t timeout_seconds, router& router) :
    socket_(io_service), body_(INIT_BUF_SIZE), timer_(io_service), timeout_seconds_(timeout_seconds),
    has_closed_(false), router_(router) {
    }

    ~connection() {
        close();
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

    void response(uint64_t req_id, std::string data, request_type req_type = request_type::req_res) {
        assert(data.size() < MAX_BUF_LEN);
        auto sp_data                   = std::make_shared<std::string>(std::move(data));
        std::weak_ptr<connection> weak = shared_from_this();
        asio::post(socket_.get_executor(), [this, weak, sp_data, req_id, req_type] {
            auto conn = weak.lock();
            if (conn) {
                response_interal(req_id, std::move(sp_data), req_type);
            }
        });
    }

    template <typename T>
    void pack_and_response(uint64_t req_id, T data) {
        auto result = msgpack_codec::pack_args_str(result_code::OK, std::move(data));
        response(req_id, std::move(result));
    }

    void set_conn_id(int64_t id) {
        conn_id_ = id;
    }

    int64_t conn_id() const {
        return conn_id_;
    }

    template <typename T>
    void set_user_data(const T& data) {
        user_data_ = data;
    }

    template <typename T>
    T get_user_data() {
        return std::any_cast<T>(user_data_);
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

    void publish(const std::string& key, const std::string& data) {
        auto result = msgpack_codec::pack_args_str(result_code::OK, key, data);
        response(0, std::move(result), request_type::sub_pub);
    }

    void set_callback(std::function<void(std::string, std::string, std::weak_ptr<connection>)> callback) {
        callback_ = std::move(callback);
    }

    void set_delay(bool delay) {
        delay_ = delay;
    }

    void on_network_error(std::function<void(std::shared_ptr<connection>, std::string)>& on_net_err) {
        on_net_err_ = &on_net_err;
    }
#ifndef RPC_DISABLE_SSL
    void enable_ssl(rpc_server_ssl_config ssl_config) {
        std::swap(ssl_config_, ssl_config);
    }
#endif
    void abort() {
        close();
        cancel_timer();
    }

private:
    void do_ssl_handshake(const char* preread_data, std::size_t read_len) {
        // handle ssl
#ifndef RPC_DISABLE_SSL
        auto self = this->shared_from_this();
        unsigned long ssl_options =
            asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use;
        try {
            asio::ssl::context ssl_context(asio::ssl::context::sslv23);
            ssl_context.set_options(ssl_options);
            ssl_context.set_password_callback(
                [cb = ssl_config_.passwd_cb](std::size_t size, asio::ssl::context_base::password_purpose purpose) {
                    return cb(std::to_string(size) + " " + std::to_string(static_cast<std::size_t>(purpose)));
                });

            ssl_context.use_certificate_chain_file(ssl_config_.certificate_path);
            ssl_context.use_private_key_file(ssl_config_.private_key_path, asio::ssl::context::pem);
            if (!ssl_config_.tmp_dh_path.empty()) ssl_context.use_tmp_dh_file(ssl_config_.tmp_dh_path);
            ssl_stream_ = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket&>>(socket_, ssl_context);
        } catch (const std::exception& e) {
            if (on_net_err_) {
                (*on_net_err_)(self, e.what());
            }
            close();
            return;
        }

        ssl_stream_->async_handshake(asio::ssl::stream_base::server, asio::const_buffer(preread_data, read_len),
                                     [this, self](const asio::error_code& error, std::size_t) {
                                         if (error) {
                                             print(error);
                                             if (on_net_err_) {
                                                 (*on_net_err_)(self, error.message());
                                             }
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
                                 if (on_net_err_) {
                                     (*on_net_err_)(self, "socket already closed");
                                 }
                                 return;
                             }
                             do {
                                 if (ec) {
                                     print(ec);
                                     if (on_net_err_) {
                                         (*on_net_err_)(self, ec.message());
                                     }

                                     break;
                                 }
                                 const std::uint8_t* p_data = (const std::uint8_t*)head_;
                                 if (p_data[0] == 0x16) // ssl Handshake
                                 {
                                     if (p_data[1] != 0x03 || p_data[2] > 0x03) {
                                         if (on_net_err_) {
                                             (*on_net_err_)(self, "rpc only support SSL 3.0 to TLS 1.2");
                                         }
                                         break;
                                     }
                                     do_ssl_handshake(head_, kSslPreReadSize);
                                 } else {
#ifdef RPC_DEBUG
                                     std::cout << "[rpc] WARNNING!!! falling to no ssl socket" << std::endl;
#endif
                                     ssl_config_.certificate_path.clear();
                                     read_head(kSslPreReadSize);
                                 }
                                 return;
                             } while (0);
                             close();
                         });
    }

    bool is_ssl() const {
#ifndef RPC_DISABLE_SSL
        return !ssl_config_.certificate_path.empty();
#else
        return false;
#endif
    }
    void read_head(std::size_t offset = 0) {
        reset_timer();
        auto self(this->shared_from_this());
        async_read_head(head_ + offset, HEAD_LEN - offset, [this, self](asio::error_code ec, std::size_t length) {
            if (!socket_.is_open()) {
                if (on_net_err_) {
                    (*on_net_err_)(self, "socket already closed");
                }
                return;
            }

            if (!ec) {
                // const uint32_t body_len = *((int*)(head_));
                // req_id_ = *((std::uint64_t*)(head_ + sizeof(int32_t)));
                rpc_header* header = (rpc_header*)(head_);
                if (header->magic != MAGIC_NUM) {
                    print("protocol error");
                    close();
                    return;
                }

                req_id_                 = header->req_id;
                const uint32_t body_len = header->body_len;
                req_type_               = header->req_type;
                if (body_len > 0 && body_len < MAX_BUF_LEN) {
                    if (body_.size() < body_len) {
                        body_.resize(body_len);
                    }
                    read_body(header->func_id, body_len);
                    return;
                }
                if (header->func_id > 0) {
                    handle_none_param_request(header->func_id);
                    return;
                }
                if (body_len == 0) { // nobody, just head, maybe as heartbeat.
                    cancel_timer();
                    read_head();
                } else {
                    print("invalid body len");
                    if (on_net_err_) {
                        (*on_net_err_)(self, "invalid body len");
                    }
                    close();
                }
            } else {
                print(ec);
                if (on_net_err_) {
                    (*on_net_err_)(self, ec.message());
                }
                close();
            }
        });
    }

    void read_body(uint32_t func_id, std::size_t size) {
        auto self(this->shared_from_this());
        async_read(size, [this, func_id, self](asio::error_code ec, std::size_t length) {
            cancel_timer();

            if (!socket_.is_open()) {
                if (on_net_err_) {
                    (*on_net_err_)(self, "socket already closed");
                }
                return;
            }

            if (!ec) {
                read_head();
                if (req_type_ == request_type::req_res) {
                    process_request(func_id, body_.data(), length);
                } else if (req_type_ == request_type::sub_pub) {
                    try {
                        msgpack_codec codec;
                        auto p = codec.unpack_tuple<std::tuple<std::string, std::string>>(body_.data(), length);
                        callback_(std::move(std::get<0>(p)), std::move(std::get<1>(p)), this->shared_from_this());
                    } catch (const std::exception& ex) {
                        print(ex);
                        if (on_net_err_) {
                            (*on_net_err_)(self, ex.what());
                        }
                    }
                }
            } else {
                if (on_net_err_) {
                    (*on_net_err_)(self, ec.message());
                }
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
            response_interal(req_id_, std::make_shared<std::string>(std::move(ret.result)));
        }
    }
    void response_interal(uint64_t req_id, std::shared_ptr<std::string> data,
                          request_type req_type = request_type::req_res) {
        assert(data->size() < MAX_BUF_LEN);

        write_queue_.emplace_back(message_type { req_id, req_type, std::move(data) });
        if (write_queue_.size() > 1) {
            return;
        }

        write();
    }

    void write() {
        auto& msg   = write_queue_.front();
        write_size_ = (uint32_t)msg.content->size();
        header_     = { MAGIC_NUM, msg.req_type, write_size_, msg.req_id };
        std::array<asio::const_buffer, 2> write_buffers;
        write_buffers[0] = asio::buffer(&header_, sizeof(rpc_header));
        write_buffers[1] = asio::buffer(msg.content->data(), write_size_);

        auto self = this->shared_from_this();
        async_write(write_buffers, [this, self](asio::error_code ec, std::size_t length) { on_write(ec, length); });
    }

    void on_write(asio::error_code ec, std::size_t length) {
        if (ec) {
            print(ec);
            if (on_net_err_) {
                (*on_net_err_)(shared_from_this(), ec.message());
            }
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
    }

    template <typename Handler>
    void async_read_head(char* buf, std::size_t len, Handler handler) {
        if (is_ssl()) {
#ifndef RPC_DISABLE_SSL
            asio::async_read(*ssl_stream_, asio::buffer(buf, len), std::move(handler));
#endif
        } else {
            asio::async_read(socket_, asio::buffer(buf, len), std::move(handler));
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

        if (has_closed_) {
            return;
        }
#ifndef RPC_DISABLE_SSL
        if (ssl_stream_) {
            asio::error_code ec;
            ssl_stream_->shutdown(ec);
            ssl_stream_ = nullptr;
        }
#endif
        asio::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
        has_closed_ = true;
    }

    template <typename... Args>
    void print(Args... args) {
#ifdef RPC_DEBUG
        std::cout << "[rpc] ";
        std::initializer_list<int> { (std::cout << args << ' ', 0)... };
        std::cout << "\n";
#endif
    }

    void print(const asio::error_code& ec) {
        print(ec.value(), ec.message());
    }

    void print(const std::exception& ex) {
        print(ex.what());
    }

    tcp::socket socket_;
    char head_[HEAD_LEN];
    std::vector<char> body_;
    std::uint64_t req_id_;
    request_type req_type_;

    rpc_header header_;

    uint32_t write_size_ = 0;

    asio::steady_timer timer_;
    std::size_t timeout_seconds_;
    int64_t conn_id_ = 0;
    bool has_closed_;

    std::deque<message_type> write_queue_;
    std::function<void(std::string, std::string, std::weak_ptr<connection>)> callback_;
    std::function<void(std::shared_ptr<connection>, std::string)>* on_net_err_ = nullptr;
    router& router_;
    std::any user_data_;
    bool delay_ = false;
#ifndef RPC_DISABLE_SSL
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket&>> ssl_stream_ = nullptr;
    rpc_server_ssl_config ssl_config_;
#endif
};
} // namespace rpc_service
} // namespace network

#endif // REST_RPC_CONNECTION_H_
