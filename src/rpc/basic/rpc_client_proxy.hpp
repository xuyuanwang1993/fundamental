#pragma once
#include "use_asio.hpp"
#include <cstdint>
#include <functional>

#include "const_vars.h"
namespace network
{
namespace rpc_service
{
class rpc_client;
class RpcClientProxyInterface : public std::enable_shared_from_this<RpcClientProxyInterface> {
    friend class rpc_client;

public:
    enum HandShakeStatusMask : std::int32_t
    {
        // when get this status,you should send sendBuf's data to remote
        HandShakeDataPending = 1,
        // when get this status,you should  recv sendBuf.size() bytes  data from remote
        HandShakeNeedMoreData = 2,
        // when all thing has done return this status
        HandShakeSucess = 4,
        // when an error occurred ,return this status
        HandShakeFailed = 8
    };
    virtual ~RpcClientProxyInterface() {
    }

protected:
    // this function will be called when send buf was sent finished
    // implementation should update curentStatus
    virtual std::int32_t FinishSend() = 0;
    // this function will be called when recv buf was filled with remote data
    // implementation should update curentStatus
    virtual std::int32_t FinishRecv() = 0;
    // this function will be called at the beginning of every proxy request started
    virtual void Init() = 0;

private:
    void init(const std::function<void()>& success_cb,
              const std::function<void(const asio::error_code&)>& failed_cb,
              asio::ip::tcp::socket* sock) {
        success_cb_ = success_cb;
        failed_cb_  = failed_cb;
        ref_socket_ = sock;
        Init();
    }
    void perform() {
        if (curentStatus & HandShakeSucess) {
            on_success();
            return;
        }
        if (curentStatus & HandShakeFailed) {
            on_success();
            return;
        }
        if (curentStatus & HandShakeNeedMoreData) {
            read_data();
        }
        if (curentStatus & HandShakeDataPending) {
            write_data();
        }
    }
    void on_failed() {
        if (failed_cb_) failed_cb_(err);
    }
    void on_success() {
        if (success_cb_) success_cb_();
    }
    void write_data() {
        auto buffer = asio::const_buffer(sendBufCache.data(), sendBufCache.size());
        asio::async_write(*ref_socket_, std::move(buffer),
                          [this, ptr = shared_from_this()](const asio::error_code& ec, std::size_t) {
                              if (!reference_.is_valid()) {
                                  return;
                              }

                              if (ec) {
                                  err          = ec;
                                  curentStatus = HandShakeFailed;
                              } else {
                                  curentStatus = FinishSend();
                              }
                              perform();
                          });
    }
    void read_data() {
        auto buffer = asio::mutable_buffer(recvBufCache.data(), recvBufCache.size());
        asio::async_read(*ref_socket_, std::move(buffer),
                         [this, ptr = shared_from_this()](const asio::error_code& ec, std::size_t) {
                             if (!reference_.is_valid()) {
                                 return;
                             }
                             if (ec) {
                                 err          = ec;
                                 curentStatus = HandShakeFailed;
                             } else {
                                 curentStatus = FinishRecv();
                             }
                             perform();
                         });
    }

    void release_obj() {
        reference_.release();
        asio::post(ref_socket_->get_executor(), [this, ref = shared_from_this()] {
            std::error_code ec;
            ref_socket_->close(ec);
        });
    }

protected:
    rpc_data_reference reference_;
    std::int32_t curentStatus = HandShakeDataPending;
    // handshake
    /// if curentStatus&HandShakeDataPending!=0,you should filled this buffer
    std::vector<std::uint8_t> sendBufCache;
    /// if curentStatus&HandShakeNeedMoreData!=0,you should give recvBuf a initialization with a none zero size
    std::vector<std::uint8_t> recvBufCache;

private:
    asio::error_code err;
    std::function<void()> success_cb_;
    std::function<void(const asio::error_code&)> failed_cb_;
    asio::ip::tcp::socket* ref_socket_ = nullptr;
    ;
};
} // namespace rpc_service
} // namespace network
