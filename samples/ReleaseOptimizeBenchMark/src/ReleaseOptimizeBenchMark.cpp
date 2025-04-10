
#include "benchmark/benchmark.h"

#define BASIC_BENCHMARK_TEST(x) BENCHMARK(x)->Arg(8)->Arg(512)->Arg(8192)

double test_func(double x, double y) {
    double ret = 0.0;
    for (std::size_t i = 0; i < 10000; ++i) {
        ret *= x;
        ret += y;
    }
    return ret;
};

void BM_Optimize(benchmark::State& state) {
    for (auto _ : state) {
        auto ret = test_func(1.1, 2.1);
        benchmark::DoNotOptimize(ret);
    }
}
BENCHMARK(BM_Optimize);

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
