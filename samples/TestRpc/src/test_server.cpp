#include "test_server.h"

#include "rpc/basic/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
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

static Fundamental::DelayQueue s_delay_queue;

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

// if you want to response later, you can use async model, you can control when
// to response
void delay_echo(rpc_conn conn, const std::string& src) {
    FINFO("delay echo request:{}",src);
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
    FINFO("echo request:{}",src);
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
void server_task() {

    rpc_server server(9000, std::thread::hardware_concurrency(), 3600);
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
    std::thread t2([]() {
        while (!stop) {
            std::this_thread::yield();
            s_delay_queue.HandleEvent();
        }
    });
    server.run();
    stop = true;
    thd.join();
    t2.join();
}

void run_server() {
    s_thread = std::make_unique<std::thread>(server_task);
}

void exit_server() {
    stop = false;
    if (p_server) p_server->post_stop();
    if (s_thread) s_thread->join();
}