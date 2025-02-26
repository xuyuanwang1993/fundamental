

#include "fundamental/basic/allocator.hpp"
#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"

#include "rpc/basic/custom_rpc_proxy.hpp"
#include "rpc/basic/rpc_client.hpp"

#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <string>

#include "test_server.h"

#include "benchmark/benchmark.h"

template <std::size_t blockSize>
static void TestNormal(benchmark::State& state) {
    network::rpc_service::rpc_client client("127.0.0.1", 9000);
    client.connect();
    std::string msg(blockSize, 'a');
    if (blockSize < 1024 * 1024 * 32) {
        auto msg_recv = client.call<20000, std::string>("echo", msg);
        FASSERT(msg_recv == msg);
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(client.call<20000, std::string>("echo", msg));
    }
}

template <std::size_t blockSize>
static void TestProxy(benchmark::State& state) {
    network::rpc_service::rpc_client client("127.0.0.1", std::stoul(kProxyServicePort));
    client.set_proxy(std::make_shared<network::rpc_service::CustomRpcProxy>(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    client.connect();
    std::string msg(blockSize, 'a');
    if (blockSize < 1024 * 1024 * 32) {
        auto msg_recv = client.call<20000, std::string>("echo", msg);
        FASSERT(msg_recv == msg);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(client.call<20000, std::string>("echo", msg));
    }
}
#ifndef RPC_DISABLE_SSL
template <std::size_t blockSize>
static void TestSslProxy(benchmark::State& state) {
    network::rpc_service::rpc_client client("127.0.0.1", std::stoul(kProxyServicePort));
    client.enable_ssl("server.crt");
    client.set_proxy(std::make_shared<network::rpc_service::CustomRpcProxy>(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    client.connect();
    std::string msg(blockSize, 'a');
    if (blockSize < 1024 * 1024 * 32) {
        auto msg_recv = client.call<20000, std::string>("echo", msg);
        FASSERT(msg_recv == msg);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(client.call<30000, std::string>("echo", msg));
    }
}
template <std::size_t blockSize>
static void TestSslProxyStream(benchmark::State& state) {
    network::rpc_service::rpc_client client("127.0.0.1", std::stoul(kProxyServicePort));
    client.enable_ssl("server.crt");
    client.set_proxy(std::make_shared<network::rpc_service::CustomRpcProxy>(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    client.connect();
    std::string msg(blockSize, 'a');
    std::string recv(blockSize, 'b');
    auto stream = client.upgrade_to_stream("echos");
    if (blockSize < 1024 * 1024 * 32) {
        stream->Write(msg);
        stream->Read(recv, 0);
        FASSERT(msg == recv);
    }

    for (auto _ : state) {
        stream->Write(msg);
        stream->Read(recv, 0);
    }
    stream->WriteDone();
    stream->Finish();
}
#endif
BENCHMARK_TEMPLATE(TestNormal, 0);
BENCHMARK_TEMPLATE(TestNormal, 1024);
BENCHMARK_TEMPLATE(TestNormal, 4096);
BENCHMARK_TEMPLATE(TestNormal, 8192);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 32);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 128);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 1024);

BENCHMARK_TEMPLATE(TestProxy, 0);
BENCHMARK_TEMPLATE(TestProxy, 1024);
BENCHMARK_TEMPLATE(TestProxy, 4096);
BENCHMARK_TEMPLATE(TestProxy, 8192);
BENCHMARK_TEMPLATE(TestProxy, 1024 * 1024);
BENCHMARK_TEMPLATE(TestProxy, 1024 * 1024 * 32);
BENCHMARK_TEMPLATE(TestProxy, 1024 * 1024 * 128);
BENCHMARK_TEMPLATE(TestProxy, 1024 * 1024 * 1024);
#ifndef RPC_DISABLE_SSL
BENCHMARK_TEMPLATE(TestSslProxy, 1);
BENCHMARK_TEMPLATE(TestSslProxy, 1024);
BENCHMARK_TEMPLATE(TestSslProxy, 4096);
BENCHMARK_TEMPLATE(TestSslProxy, 8192);
BENCHMARK_TEMPLATE(TestSslProxy, 1024 * 1024);
BENCHMARK_TEMPLATE(TestSslProxy, 1024 * 1024 * 32);
BENCHMARK_TEMPLATE(TestSslProxy, 1024 * 1024 * 128);
BENCHMARK_TEMPLATE(TestSslProxy, 1024 * 1024 * 1024);

BENCHMARK_TEMPLATE(TestSslProxyStream, 1);
BENCHMARK_TEMPLATE(TestSslProxyStream, 1024);
BENCHMARK_TEMPLATE(TestSslProxyStream, 4096);
BENCHMARK_TEMPLATE(TestSslProxyStream, 8192);
BENCHMARK_TEMPLATE(TestSslProxyStream, 1024 * 1024);
BENCHMARK_TEMPLATE(TestSslProxyStream, 1024 * 1024 * 32);
BENCHMARK_TEMPLATE(TestSslProxyStream, 1024 * 1024 * 128);
BENCHMARK_TEMPLATE(TestSslProxyStream, 1024 * 1024 * 1024);
#endif
int main(int argc, char* argv[]) {
    int mode = 0;
    if (argc > 1) mode = std::stoi(argv[1]);
    Fundamental::fs::SwitchToProgramDir(argv[0]);
    argc=0;
    argv=nullptr;
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel = Fundamental::LogLevel::info;

    Fundamental::Logger::Initialize(std::move(options));
    if (mode == 0) {
        char arg0_default[] = "benchmark";
        char* args_default  = arg0_default;
        if (!argv) {
            argc = 1;
            argv = &args_default;
        }
        ::benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
        run_server();
        ::benchmark::RunSpecifiedBenchmarks();
        ::benchmark::Shutdown();
        exit_server();
    } else if (mode == 1) {
        std::promise<void> sync_p;
        server_task(sync_p);
        sync_p.get_future().get();
        exit_server();
    } else {
        char arg0_default[] = "benchmark";
        char* args_default  = arg0_default;
        if (!argv) {
            argc = 1;
            argv = &args_default;
        }
                network::io_context_pool::s_excutorNums = 10;
        network::io_context_pool::Instance().start();
        ::benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
        ::benchmark::RunSpecifiedBenchmarks();
        ::benchmark::Shutdown();
    }
    return 0;
}