#include "test_server.h"

#include "rpc/basic/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/services/proxy_server/proxy_connection.hpp"
#include "network/services/proxy_server/proxy_defines.h"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_connection.hpp"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_defines.h"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_manager.hpp"

using namespace network;
using namespace rpc_service;

RTTR_REGISTRATION {
    rttr::registration::class_<person>("person")
        .constructor()(rttr::policy::ctor::as_object)
        .property("id", &person::id)
        .property("age", &person::age)
        .property("name", &person::name);
    rttr::registration::class_<dummy1>("dummy1")
        .constructor()(rttr::policy::ctor::as_object)
        .property("id", &dummy1::id)
        .property("str", &dummy1::str);
}

struct dummy {
    int add(rpc_conn conn, int a, int b) {
        return a + b;
    }
};

static Fundamental::DelayQueue& s_delay_queue = *(Fundamental::Application::Instance().DelayQueue());

std::string translate(rpc_conn conn, const std::string& orignal) {
    std::string temp = orignal;
    for (auto& c : temp) {
        c = std::toupper(c);
    }
    return temp;
}

void hello(rpc_conn conn, const std::string& str) {
    FINFOS << "hello from client:" << str;
}

std::string get_person_name(rpc_conn conn, const person& p) {
    return p.name;
}

person get_person(rpc_conn conn) {
    return { 1, "tom", 20 };
}

void upload(rpc_conn conn, const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    file.write(content.data(), content.size());
}

std::string download(rpc_conn conn, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return "";
    }

    file.seekg(0, std::ios::end);
    size_t file_len = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string content;
    content.resize(file_len);
    file.read(&content[0], file_len);

    return content;
}

std::string get_name(rpc_conn conn, const person& p) {
    return p.name;
}

void auto_disconnect(rpc_conn conn, std::int32_t v) {
    FINFOS << "auto_disconnect " << v;
    conn.lock()->abort();
}

// if you want to response later, you can use async model, you can control when
// to response
void delay_echo(rpc_conn conn, const std::string& src) {
    FINFO("delay echo request:{}", src);
    auto sp = conn.lock();
    sp->set_delay(true);
    auto req_id = sp->request_id(); // note: you need keep the request id at that

    auto h = s_delay_queue.AddDelayTask(
        50,
        [conn, req_id, src] {
            auto conn_sp = conn.lock();
            if (conn_sp) {
                conn_sp->pack_and_response(req_id, std::move(src));
            }
        },
        true);
    s_delay_queue.StartDelayTask(h);
}

std::string echo(rpc_conn conn, const std::string& src) {
    return src;
}

int get_int(rpc_conn conn, int val) {
    return val;
}

dummy1 get_dummy(rpc_conn conn, dummy1 d) {
    return d;
}
static bool stop = false;
static std::unique_ptr<std::thread> s_thread;
rpc_server* p_server = nullptr;
void server_task(std::promise<void>& sync_p) {

    rpc_server server(9000, std::thread::hardware_concurrency(), 3600);
    server.enable_ssl({ nullptr, "server.crt", "server.key", "dh2048.pem" });
    p_server = &server;
    dummy d;
    server.register_handler("add", &dummy::add, &d);

    server.register_handler("get_dummy", get_dummy);

    server.register_handler("translate", translate);
    server.register_handler("hello", hello);
    server.register_handler("get_person_name", get_person_name);
    server.register_handler("get_person", get_person);
    server.register_handler("upload", upload);
    server.register_handler("download", download);
    server.register_handler("get_name", get_name);
    server.register_handler("delay_echo", delay_echo);
    server.register_handler("echo", echo);
    server.register_handler("get_int", get_int);
    server.register_handler("auto_disconnect", auto_disconnect);
    server.register_handler(
        "publish_by_token", [&server](rpc_conn conn, std::string key, std::string token, std::string val) {
            FINFOS << "publish_by_token:" << key << ":" << token << " " << Fundamental::Utils::BufferToHex(val);
            server.forward_publish_msg(std::move(key), std::move(val), std::move(token));
        });

    server.register_handler("publish", [&server](rpc_conn conn, std::string key, std::string token, std::string val) {
        FINFOS << "publish:" << key << ":" << token << " " << Fundamental::Utils::BufferToHex(val);
        server.forward_publish_msg(std::move(key), std::move(val));
    });
    server.set_network_err_callback([](std::shared_ptr<connection> conn, std::string reason) {
        std::cout << "remote client address: " << conn->remote_address() << " networking error, reason: " << reason
                  << "\n";
    });

    std::thread thd([&server] {
        person p { 10, "jack", 21 };
        while (!stop) {
            server.publish("key", "hello subscriber from server");
            auto list = server.get_token_list();
            for (auto& token : list) {
                server.publish_by_token("key", token, p);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    std::thread t2([&]() {
        network::io_context_pool::s_excutorNums = 10;
        network::io_context_pool::Instance().start();
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
        network::proxy::ProxyServer s(network::MakeTcpEndpoint(std::stoul(kProxyServicePort)));
        s.GetHandler().RegisterProtocal(network::proxy::kTrafficProxyOpcode,
                                        network::proxy::TrafficProxyConnection::MakeShared);
        s.Start();
        sync_p.set_value();
        Fundamental::Application::Instance().Loop();
    });

    server.run();
    stop = true;
    thd.join();
    t2.join();
}

void run_server() {
    std::promise<void> sync_p;
    s_thread = std::make_unique<std::thread>([&]() { server_task(sync_p); });
    sync_p.get_future().get();
}

void exit_server() {
    stop = false;
    Fundamental::Application::Instance().Exit();
    if (p_server) p_server->post_stop();
    if (s_thread) s_thread->join();
}