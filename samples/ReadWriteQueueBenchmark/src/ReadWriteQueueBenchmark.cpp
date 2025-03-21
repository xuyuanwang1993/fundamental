

#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "fundamental/read_write_queue/queue_with_locker.hpp"
#include "fundamental/read_write_queue/readerwritercircularbuffer.h"
#include "fundamental/read_write_queue/readerwriterqueue.h"
#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "benchmark/benchmark.h"

void LockFreeSigleThread(benchmark::State& state) {
    Fundamental::BlockingReaderWriterCircularBuffer<std::size_t> q(31);
    std::size_t t = 0;
    for (auto _ : state) {
        q.wait_enqueue(0);
        q.wait_dequeue(t);
    }
}

void LockFreeQueueSigleThread(benchmark::State& state) {
    Fundamental::ReaderWriterQueue<std::size_t> q(31);
    std::size_t t = 0;
    for (auto _ : state) {
        q.try_enqueue(0);
        q.try_dequeue(t);
    }
}

void LockSigleThread(benchmark::State& state) {
    Fundamental::QueueMPSC<std::size_t> q(31);
    std::size_t t = 0;
    for (auto _ : state) {
        q.Push(t);
        q.Pop(t);
    }
}

void LockFreeMultiThread(benchmark::State& state) {
    Fundamental::BlockingReaderWriterCircularBuffer<std::size_t> q(31);

    bool finished = false;
    std::thread t([&]() {
        std::size_t i = 0;
        while (!finished) {
            q.try_enqueue(++i);
        }
    });
    std::size_t value = 0;
    for (auto _ : state) {
        q.wait_dequeue(value);
    }
    finished = true;
    t.join();
}

void LockFreeQueueMultiThread(benchmark::State& state) {
    Fundamental::ReaderWriterQueue<std::size_t> q(31);

    bool finished = false;
    std::thread t([&]() {
        std::size_t i = 0;
        while (!finished) {
            q.try_enqueue(++i);
        }
    });
    std::size_t value = 0;
    for (auto _ : state) {
        while (!q.try_dequeue(value)) {
        };
    }
    finished = true;
    t.join();
}

void LockMultiThread(benchmark::State& state) {
    Fundamental::QueueMPSC<std::size_t> q;
    bool finished = false;
    std::thread t([&]() {
        std::size_t i = 0;
        while (!finished) {
            q.Push(++i);
        }
    });
    std::size_t value = 0;
    for (auto _ : state) {
        q.Pop(value);
    }
    finished = true;
    t.join();
}

BENCHMARK(LockFreeSigleThread)->Iterations(10000000);

BENCHMARK(LockFreeQueueSigleThread)->Iterations(10000000);
BENCHMARK(LockSigleThread)->Iterations(10000000);

BENCHMARK(LockFreeMultiThread)->Iterations(10000000);
BENCHMARK(LockFreeQueueMultiThread)->Iterations(10000000);
BENCHMARK(LockMultiThread)->Iterations(10000000);
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