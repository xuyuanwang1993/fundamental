#include "test_server.h"

#include "rpc/basic/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"

#include "fundamental/application/application.hpp"
#include "network/services/proxy_server/proxy_connection.hpp"
#include "network/services/proxy_server/proxy_defines.h"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_connection.hpp"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_defines.h"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_manager.hpp"

using namespace network;
using namespace rpc_service;
static auto& rpc_stream_pool = Fundamental::ThreadPool::Instance<100>();
std::string echo(rpc_conn conn, const std::string& src) {
    return src;
}

void echos(rpc_conn conn) {
    auto c = conn.lock();
    auto w = c->InitRpcStream();

    rpc_stream_pool.Enqueue(
        [](decltype(w) stream) {
            std::string msg;
            while (stream->Read(msg, 0)) {
                if (!stream->Write(msg)) break;
            };
            stream->WriteDone();
            stream->Finish(0);
        },
        w);
}

static std::unique_ptr<std::thread> s_thread;
rpc_server* p_server = nullptr;
void server_task(std::promise<void>& sync_p) {

    rpc_server server(9000, 3600);
    server.enable_ssl({ nullptr, "server.crt", "server.key", "dh2048.pem" });
    p_server = &server;
    server.register_handler("echo", echo);
    server.register_handler("echos", echos);
    network::io_context_pool::s_excutorNums = 10;
    network::io_context_pool::Instance().start();
    Fundamental::Application::Instance().exitStarted.Connect([&]() { network::io_context_pool::Instance().stop(); });
    network::io_context_pool::Instance().notify_sys_signal.Connect(
        [](std::error_code code, std::int32_t signo) { Fundamental::Application::Instance().Exit(); });
    {
        using namespace network::proxy;
        auto& manager = TrafficProxyManager::Instance();
        { // add http proxy
            TrafficProxyHostInfo host;
            host.token = kProxyServiceToken;
            {
                TrafficProxyHost hostRecord;
                hostRecord.host    = "0.0.0.0";
                hostRecord.service = "9000";
                host.hosts.emplace(TrafficProxyDataType(kProxyServiceField), std::move(hostRecord));
            }
            manager.UpdateTrafficProxyHostInfo(TrafficProxyDataType(kProxyServiceName), std::move(host));
        }
    }
    // Initialise the server.
    using asio::ip::tcp;
    tcp::resolver resolver(network::io_context_pool::Instance().get_io_context());
    auto endpoints = resolver.resolve("0.0.0.0", kProxyServicePort);
    if (endpoints.empty()) {
        FERR("resolve failed");
        return;
    }
    network::proxy::ProxyServer s(*endpoints.begin());
    s.GetHandler().RegisterProtocal(network::proxy::kTrafficProxyOpcode,
                                    network::proxy::TrafficProxyConnection::MakeShared);
    s.Start();
    server.start();
    rpc_stream_pool.Spawn(5);
    sync_p.set_value();
    Fundamental::Application::Instance().Loop();
}

void run_server() {
    std::promise<void> sync_p;
    s_thread = std::make_unique<std::thread>([&]() { server_task(sync_p); });
    sync_p.get_future().wait();
}

void exit_server() {
    Fundamental::Application::Instance().Exit();
    if (p_server) p_server->post_stop();
    if (s_thread) s_thread->join();
}