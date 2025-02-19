

#include "fundamental/basic/allocator.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
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
    network::rpc_client client("127.0.0.1", 9000);
    client.connect();
    client.enable_auto_reconnect();
    client.enable_auto_heartbeat();
    std::string msg(blockSize, 'a');
    auto result = client.call<std::string>("echo", msg);
    for (auto _ : state) {
        benchmark::DoNotOptimize(client.call<std::string>("echo", msg));
    }
}

BENCHMARK_TEMPLATE(TestNormal, 0);
BENCHMARK_TEMPLATE(TestNormal, 1024);
BENCHMARK_TEMPLATE(TestNormal, 4096);
BENCHMARK_TEMPLATE(TestNormal, 8192);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 32);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 128);
BENCHMARK_TEMPLATE(TestNormal, 1024 * 1024 * 1024);
int main(int argc, char* argv[]) {
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
    return 0;
}