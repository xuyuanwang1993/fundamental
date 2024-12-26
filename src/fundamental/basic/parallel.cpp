#include "parallel.hpp"
#include <atomic>
namespace Fundamental::internal {
static void InitParallelThreadPool() {
    std::size_t threadNums = std::thread::hardware_concurrency();
    try {
        auto ptr = ::getenv("F_PARALLEL_THREADS");
        if (ptr) {
            auto value = std::max(static_cast<std::size_t>(1), static_cast<std::size_t>(std::stoul(ptr)));
            threadNums = std::min(threadNums, value);
        }
    } catch (const std::exception&) {
    }
    auto& pool = GetParallelThreadPool();
    pool.Spawn(threadNums);
}

void _InitThreadPool() {
    static std::once_flag flag;
    std::call_once(flag, InitParallelThreadPool);
}
} // namespace Fundamental::internal
