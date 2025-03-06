#include "test_server.h"

#include "rpc/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"

#include "fundamental/application/application.hpp"

using namespace network;
using namespace rpc_service;
static auto& rpc_stream_pool = Fundamental::ThreadPool::Instance<100>();
std::string echo(rpc_conn conn, std::string src) {
    if (src.size() > 4096) {
        auto sp = conn.lock();
        sp->set_delay(true);
        auto req_id = sp->request_id(); // note: you need keep the request id at that
        rpc_stream_pool.Enqueue([src = std::move(src), conn, req_id]() ->void {
            auto sp = conn.lock();
            if (sp) sp->response(req_id, network::rpc_service::request_type::rpc_res, std::move(src));
        });
        return "";
    }
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

std::unique_ptr<std::thread> s_thread;
static network::proxy::ProxyManager s_manager;

void server_task(std::promise<void>& sync_p) {

    auto s_server = rpc_server::make_shared(9000);
    s_server->enable_ssl({ nullptr, "server.crt", "server.key", "dh2048.pem" });
    s_server->register_handler("echo", echo);
    s_server->register_handler("echos", echos);
    network::init_io_context_pool(10);
    {
        using namespace network::proxy;
        auto& manager = s_manager;
        { // add http proxy
            ProxyHostInfo host;
            host.token = kProxyServiceToken;
            {
                ProxyHost hostRecord;
                hostRecord.host    = "0.0.0.0";
                hostRecord.service = "9000";
                host.hosts.emplace(kProxyServiceField, std::move(hostRecord));
            }
            manager.UpdateProxyHostInfo(kProxyServiceName, std::move(host));
        }
    }
    s_server->enable_data_proxy(&s_manager);
    s_server->start();
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
    if (s_thread) s_thread->join();
}