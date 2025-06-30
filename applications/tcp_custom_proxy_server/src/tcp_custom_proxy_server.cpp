

#include "rpc/rpc_server.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#include "fundamental/application/application.hpp"
#include "fundamental/basic/arg_parser.hpp"
#include "fundamental/basic/log.h"

using namespace network;
using namespace rpc_service;

int main(int argc, char* argv[]) {
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel = Fundamental::LogLevel::info;
    options.logFormat    = "%^[%L]%H:%M:%S.%e%$[%t] %v ";
    Fundamental::Logger::Initialize(std::move(options));
    Fundamental::arg_parser arg_parser { argc, argv, "1.0.1" };
    std::size_t threads = 8;
    std::size_t port    = 32000;
    arg_parser.AddOption("threads", Fundamental::StringFormat("handler's thread nums default:{}", threads), 't',
                         Fundamental::arg_parser::param_type::required_param, "number");
    arg_parser.AddOption("port", Fundamental::StringFormat("proxyserver's listening port default:{}", port), 'p',
                         Fundamental::arg_parser::param_type::required_param, "port");
    if (argc == 1) {
        arg_parser.ShowHelp();
        return 1;
    }
    if (!arg_parser.ParseCommandLine()) {
        arg_parser.ShowHelp();
        return 1;
    }
    if (arg_parser.HasParam()) {
        arg_parser.ShowHelp();
        return 1;
    }
    if (arg_parser.HasParam(arg_parser.kVersionOptionName)) {
        arg_parser.ShowVersion();
        return 1;
    }
    threads = arg_parser.GetValue("threads", threads);
    port    = arg_parser.GetValue("port", port);

    auto s_server = network::make_guard<rpc_server>(port);
    auto p        = s_server.get();
    auto& server  = *s_server.get();
    network::init_io_context_pool(threads);
    server.start();
    Fundamental::Application::Instance().Loop();
    Fundamental::Application::Instance().Exit();
    return 0;
}