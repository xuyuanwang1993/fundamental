#include "test_server.h"

#include "rpc/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "fundamental/thread_pool/thread_pool.h"

using namespace network;
using namespace rpc_service;

static auto& rpc_stream_pool = Fundamental::ThreadPool::Instance<100>();

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
                conn_sp->response(req_id, network::rpc_service::request_type::rpc_res, std::move(src));
            }
        },
        true);
    s_delay_queue.StartDelayTask(h);
}

void test_stream(rpc_conn conn) {
    auto c = conn.lock();
    auto w = c->InitRpcStream();

    rpc_stream_pool.Enqueue(
        [](decltype(w) stream) {
            FWARN("stream start");
            person p;
            while (stream->Read(p, 0)) {
                FDEBUG("id:{},age:{},name:{}", p.id, p.age, p.name);
                p.name += " from server";
                p.age += 10;
                p.id += 10;
                if (!stream->Write(p)) break;
            };
            stream->WriteDone();
            auto ec = stream->Finish(0);
            if (ec) {
                FINFO("rpc failed {}", ec.message());
            } else {
                FINFO("rpc done");
            }
        },
        w);
}

void test_read_stream(rpc_conn conn) {
    auto c = conn.lock();
    auto w = c->InitRpcStream();

    rpc_stream_pool.Enqueue(
        [](decltype(w) stream) {
            person p;
            p.id   = 0;
            p.age  = 1;
            p.name = "111";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            stream->Write(p);
            stream->WriteDone();
            auto ec = stream->Finish(0);
            if (ec) {
                FINFO("rpc failed {}", ec.message());
            } else {
                FINFO("rpc done");
            }
        },
        w);
}

void test_write_stream(rpc_conn conn) {
    auto c = conn.lock();
    auto w = c->InitRpcStream();

    rpc_stream_pool.Enqueue(
        [](decltype(w) stream) {
            person p;
            stream->WriteDone();
            while (stream->Read(p, 0)) {
                FINFO("write {} {} {}", p.id, p.age, p.name);
            }

            auto ec = stream->Finish(0);
            if (ec) {
                FINFO("rpc failed {}", ec.message());
            } else {
                FINFO("rpc done");
            }
        },
        w);
}

void test_broken_stream(rpc_conn conn) {
    auto c = conn.lock();
    auto w = c->InitRpcStream();

    rpc_stream_pool.Enqueue(
        [](decltype(w) stream) {
            // just do nothing
        },
        w);
}

void test_echo_stream(rpc_conn conn) {
    auto c  = conn.lock();
    auto w  = c->InitRpcStream();
    auto id = c->conn_id();
    rpc_stream_pool.Enqueue(
        [id](decltype(w) stream) {
            FWARN("stream start");
            std::string msg;
            while (stream->Read(msg, 0)) {
                if (!stream->Write(msg + " from server")) break;
            };
            stream->WriteDone();
            auto ec = stream->Finish(0);
            if (ec) {
                FINFO("rpc failed {} {}", id, ec.message());
            } else {
                FINFO("rpc done {}", id);
            }
        },
        w);
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
std::unique_ptr<std::thread> s_thread;
static network::proxy::ProxyManager s_manager;
void server_task(std::promise<void>& sync_p) {

    auto s_server = std::make_shared<rpc_server>(9000, 3600);
    auto& server  = *s_server;
    server.enable_ssl({ nullptr, "server.crt", "server.key", "dh2048.pem" });
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
    server.register_handler("test_stream", test_stream);
    server.register_handler("test_read_stream", test_read_stream);
    server.register_handler("test_write_stream", test_write_stream);
    server.register_handler("test_broken_stream", test_broken_stream);
    server.register_handler("test_echo_stream", test_echo_stream);
    server.on_net_err.Connect([](std::shared_ptr<connection> conn, std::string reason) {
        std::cout << "remote client address: " << conn->remote_address() << " networking error, reason: " << reason
                  << "\n";
    });
    auto time_queue = Fundamental::Application::Instance().DelayQueue();
    auto h          = time_queue->AddDelayTask(10, [s_server] {
        person p { 10, "jack_server", 21 };
        s_server->publish("key", "publish msg from server");
        s_server->publish("key_p", p);
    });
    time_queue->StartDelayTask(h);
    network::io_context_pool::s_excutorNums = 10;
    network::io_context_pool::Instance().start();
    Fundamental::Application::Instance().exitStarted.Connect([&]() { network::io_context_pool::Instance().stop(); });
    network::io_context_pool::Instance().notify_sys_signal.Connect(
        [](std::error_code code, std::int32_t signo) { Fundamental::Application::Instance().Exit(); });
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
        { // add http proxy
            ProxyHostInfo host;
            host.token = "test_http_token";
            {
                ProxyHost hostRecord;
                hostRecord.host    = "www.baidu.com";
                hostRecord.service = "http";
                host.hosts.emplace(kProxyServiceField, std::move(hostRecord));
            }
            manager.UpdateProxyHostInfo("test_http", std::move(host));
        }
    }
    server.enable_data_proxy(&s_manager);

    server.start();
    rpc_stream_pool.Spawn(5);
    sync_p.set_value();
    Fundamental::Application::Instance().exitStarted.Connect(
        [s_server = std::move(s_server), h, time_queue]() mutable {
            FDEBUG("emit stop server");
            time_queue->StopDelayTask(h);
            if (s_server) s_server->stop();
            s_server = nullptr;
        },
        false);
    Fundamental::Application::Instance().Loop();
}

void run_server() {
    std::promise<void> sync_p;
    s_thread = std::make_unique<std::thread>([&]() { server_task(sync_p); });
    sync_p.get_future().get();
}

void exit_server() {
    // delay 100ms to exit to test resource manager
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    FDEBUG("exit server 2");
    Fundamental::Application::Instance().Exit();
    FDEBUG("exit server 1");
    if (s_thread) s_thread->join();
    FDEBUG("exit server");
}
