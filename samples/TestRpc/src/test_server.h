#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <future>

struct person {
    std::int32_t id;
    std::string name;
    std::int32_t age;
};
struct dummy1 {
    std::size_t id;
    std::string str;
};

constexpr const char* kProxyServiceName  = "rpc_service";
constexpr const char* kProxyServiceField = "rpc_field";
constexpr const char* kProxyServiceToken = "rpc_token";
constexpr const char* kProxyServicePort = "9001";
void run_server();
void exit_server();
void server_task(std::promise<void>& sync_p);