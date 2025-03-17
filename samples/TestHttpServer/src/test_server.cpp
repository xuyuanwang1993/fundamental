#include "test_server.h"

#include "http/http_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <rttr/registration>
#include <thread>

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/random_generator.hpp"
#include "fundamental/delay_queue/delay_queue.h"
#include "fundamental/thread_pool/thread_pool.h"

using namespace network;
using namespace network::http;
void OnHttpBinary(std::shared_ptr<http_connection> conn, http_response& response, http_request& request) {
    constexpr std::string_view kData = "{\"code\":0,\"message\":\"hello http standalone\"}";
    response.set_content_type(".data");
    std::size_t max_count = 10;
    response.set_body_size(max_count * kData.size());
    std::size_t index = 0;
    while (index < max_count) {
        ++index;
        response.append_body(kData.data(), kData.size(),
                             [index]() { FINFO("OnHttpBinary finish send index:{}", index); });
    }
}

std::unique_ptr<std::thread> s_thread;
void server_task(std::promise<void>& sync_p) {
    http_server_config config;
    config.port                = 9000;
    config.head_case_sensitive = false;
    config.root_path           = std::filesystem::current_path().string();
    auto s_server              = network::make_guard<http_server>(config);
    s_server->enable_default_handler();
    network_server_ssl_config ssl_config;
    ssl_config.passwd_cb           = nullptr;
    ssl_config.ca_certificate_path = "ca_root.crt";
    ssl_config.private_key_path    = "server.key";
    ssl_config.certificate_path    = "server.crt";
    ssl_config.tmp_dh_path         = "dh2048.pem";
    ssl_config.verify_client       = false;
    s_server->enable_ssl(std::move(ssl_config));
    s_server->register_handler("/binary", OnHttpBinary, http::MethodFilter::HttpAll);

    network::init_io_context_pool(10);

    s_server->start();

    Fundamental::Application::Instance().Loop();
    FDEBUG("finish loop");
    sync_p.set_value();
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
