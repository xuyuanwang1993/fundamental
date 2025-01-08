

#include "fundamental/basic/allocator.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include "benchmark/benchmark.h"

struct TestObject {
    int x;
    double y;
    std::uint8_t k;
    std::uint8_t a[33];
    TestObject() {
        benchmark::DoNotOptimize(x = 1);
    }

    ~TestObject() {
        benchmark::DoNotOptimize(y = 0);
    }
};
class ObjectAllocatorBenchmark : public benchmark::Fixture {
public:
    std::vector<TestObject> vec_normal;
    std::vector<TestObject, Fundamental::ThreadSafeObjectPoolAllocator<TestObject>> vec_use_pool;
};

BENCHMARK_F(ObjectAllocatorBenchmark, normal)
(benchmark::State& state) {
    for (auto _ : state) {
        vec_normal.emplace_back();
    }
    if (vec_normal.size() > 10000) {
        vec_normal.clear();
        vec_normal.shrink_to_fit();
    }
};
BENCHMARK_F(ObjectAllocatorBenchmark, pool)
(benchmark::State& state) {
    for (auto _ : state) {
        vec_use_pool.emplace_back();
    }
    if (vec_use_pool.size() > 10000) {
        vec_use_pool.clear();
        vec_use_pool.shrink_to_fit();
    }
};
int main(int argc, char* argv[]) {
    char arg0_default[] = "benchmark";
    char* args_default  = arg0_default;
    if (!argv) {
        argc = 1;
        argv = &args_default;
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}