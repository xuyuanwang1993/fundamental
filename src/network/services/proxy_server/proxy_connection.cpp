#include "proxy_connection.hpp"
namespace network {
namespace proxy {

Connection::Connection(asio::ip::tcp::socket socket, ProxyRequestHandler& handler) :
ConnectionInterface<ProxyRequestHandler>(std::move(socket), handler),
checkTimer(socket_.get_executor(), asio::chrono::seconds(kMaxRecvRequestFrameTimeSec)) {
}

Connection::~Connection() {
    FDEBUG("~Connection");
}

void Connection::Start() {
    StartTimerCheck();
    ReadHeader();
}

void Connection::HandleClose() {
    StopTimeCheck();
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
}

void Connection::ReadHeader() {
    asio::error_code ec;
    asio::ip::tcp::no_delay option(true);
    socket_.set_option(option, ec);
    if (ec) {
        FWARN("set tcp nodelay option failed :{}", ec.message());
        return;
    }

    asio::async_read(
        socket_, asio::buffer(headerBuffer.data(), headerBuffer.size()),
        [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
            do {
                if (ec) {
                    FWARN("disconnected [{}:{}] for read header:[{}-{}] :{}",
                          socket_.remote_endpoint().address().to_string(), socket_.remote_endpoint().port(),
                          ec.category().name(), ec.value(), ec.message());
                    break;
                }
                if (!request_handler_.DecodeHeader(headerBuffer.data(), headerBuffer.size(), requestFrame)) {
                    FWARN("decode header failed");
                    break;
                }
                ReadBody();
                return;
            } while (0);
            HandleClose();
        });
}

void Connection::ReadBody() {
    asio::async_read(socket_, asio::buffer(requestFrame.payload.GetData(), requestFrame.payload.GetSize()),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         do {
                             if (ec) {
                                 FWARN("disconnected for read payload:[{}-{}] :{}", ec.category().name(), ec.value(),
                                       ec.message());
                                 break;
                             }
                             if (!request_handler_.DecodePayload(requestFrame)) {
                                 FWARN("decode payload failed");
                                 break;
                             }
                             StopTimeCheck();
                             request_handler_.UpgradeProtocal(std::forward<Connection>(*this));
                             return;
                         } while (0);
                         HandleClose();
                     });
}

void Connection::StartTimerCheck() {
    checkTimer.async_wait([this, self = shared_from_this()](const std::error_code& e) {
        if (!e) {
            FWARN("recv request frame timeout,disconnected");
            HandleClose();
        }
    });
}

void Connection::StopTimeCheck() {
    checkTimer.cancel();
}

bool ClientSession::AbortCheckPoint() {
    if (is_aborted_) {
        HandelFailed(network::error::make_error_code(network::error::proxy_errors::request_aborted));
        return true;
    } else {
        return false;
    }
}

ClientSession::ClientSession(const std::string& host, const std::string& service) :
host(host), service(service), socket_(io_context_pool::Instance().get_io_context()), resolver(socket_.get_executor()) {
}

void ClientSession::Cancel() {
    resolver.cancel();
    socket_.close();
}

void ClientSession::HandelFailed(std::error_code ec) {
    Cancel();
}

void ClientSession::Start() {
    is_aborted_.exchange(false);
    StartDnsResolve();
}

void ClientSession::Abort() {
    bool expected = false;
    if (is_aborted_.compare_exchange_strong(expected, true)) {
        Cancel();
    }
}

void ClientSession::AcquireInitLocker() {
    // spin lock wait
    while (init_fence_.test_and_set()) {
    };
}

void ClientSession::ReleaseInitLocker() {
    init_fence_.clear();
}

void ClientSession::StartDnsResolve() {
    resolver.async_resolve(
        asio::ip::tcp::v4(), host, service,
        [ref = shared_from_this(), this](asio::error_code ec, decltype(resolver)::results_type result) {
            FinishDnsResolve(ec, std::move(result));
        });
    DnsResloveStarted.Emit();
}

void ClientSession::FinishDnsResolve(asio::error_code ec, asio::ip::tcp::resolver::results_type result) {
    // wait init finished
    AcquireInitLocker();
    ReleaseInitLocker();
    DnsResloveFinished.Emit(ec, result);
    if (ec) {
        HandelFailed(ec);
        return;
    }
    if (AbortCheckPoint()) return;
    StartConnect(std::move(result));
}

void ClientSession::StartConnect(asio::ip::tcp::resolver::results_type result) {
    asio::async_connect(socket_, result,
                        [this, self = shared_from_this()](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                            asio::error_code error_code;
                            asio::ip::tcp::no_delay option(true);
                            socket_.set_option(option, error_code);
                            FinishConnect(ec, std::move(endpoint));
                        });
    ConnectStarted.Emit(result);
}

void ClientSession::FinishConnect(std::error_code ec, asio::ip::tcp::endpoint endpoint) {
    ConnectFinished(ec, endpoint);
    if (ec) {
        HandelFailed(ec);
        return;
    }
    if (AbortCheckPoint()) return;
    Process(std::move(endpoint));
}

void ClientSession::Process(asio::ip::tcp::endpoint endpoint) {
    auto address = endpoint.address();

    if (address.is_v4()) {
        FDEBUG("sample: ipv4:endpoint:{}->{}", address.to_string(), endpoint.port());
    } else {
        FDEBUG("sample: ipv6:endpoint:{}->{}", address.to_string(), endpoint.port());
    }
}
} // namespace proxy
} // namespace network