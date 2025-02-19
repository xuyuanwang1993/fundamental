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

std::string echo(rpc_conn conn, const std::string& src) {
    return src;
}

static std::unique_ptr<std::thread> s_thread;
rpc_server* p_server = nullptr;
void server_task() {

    rpc_server server(9000, std::thread::hardware_concurrency(), 3600);
    p_server = &server;
    server.register_handler("echo", echo);
    server.run();
}

void run_server() {
    s_thread = std::make_unique<std::thread>(server_task);
}

void exit_server() {
    if (p_server) p_server->post_stop();
    if (s_thread) s_thread->join();
}