#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

constexpr const char* kProxyServiceName  = "rpc_service";
constexpr const char* kProxyServiceField = "rpc_field";
constexpr const char* kProxyServiceToken = "rpc_token";
constexpr const char* kProxyServicePort = "9001";
void run_server();
void exit_server();