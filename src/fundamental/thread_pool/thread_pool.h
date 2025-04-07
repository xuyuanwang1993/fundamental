/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional> //bind
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
namespace Fundamental
{
using clock_t = std::chrono::steady_clock;

enum ThreadPoolType : std::int32_t
{
    ShortTimeThreadPool = 0,
    LongTimeThreadPool  = 1,
    BlockTimeThreadPool = 2,
    PrallelThreadPool   = 3,
    ProducerThreadPool  = 4,
    ConsumerThreadPool  = 5
};

enum ThreadPoolTaskStatus : std::uint32_t
{
    ThreadTaskWaitting,
    ThreadTaskRunning,
    ThreadTaskDone,
    ThreadTaskCancelled
};

struct ThreadPoolTaskStatusSyncerWarpper {
    std::atomic<ThreadPoolTaskStatus> status = ThreadTaskWaitting;
};

template <typename ResultType>
struct ThreadPoolTaskToken {
    bool CancelTask() {
        auto expected = ThreadPoolTaskStatus::ThreadTaskWaitting;
        return status->status.compare_exchange_strong(expected, ThreadPoolTaskStatus::ThreadTaskCancelled);
    }
    std::shared_ptr<ThreadPoolTaskStatusSyncerWarpper> status = std::make_shared<ThreadPoolTaskStatusSyncerWarpper>();
    std::future<ResultType> resultFuture;
};

struct ThreadPoolConfig {
    static constexpr std::int64_t kDefaultIdleWaitTimeMsec = 10000;
    static constexpr std::size_t kMinWorkThreadsNum        = 1;
    bool enable_auto_scaling                               = true;
    // 0 means no limit
    std::size_t max_threads_limit    = 0;
    std::size_t min_work_threads_num = kMinWorkThreadsNum;
    std::int64_t ilde_wait_time_ms   = kDefaultIdleWaitTimeMsec;
};
class ThreadPool final {
    struct Task {
        clock_t::time_point time;
        std::function<void()> func = nullptr;
        std::shared_ptr<ThreadPoolTaskStatusSyncerWarpper> status;
        bool operator>(const Task& other) const {
            return time > other.time;
        }
        bool operator<(const Task& other) const {
            return time < other.time;
        }
    };

public:
    template <std::int32_t Index = ShortTimeThreadPool>
    static ThreadPool& Instance() {
        // Init handles joining on cleanup
        static ThreadPool* instance = new ThreadPool(Index);
        return *instance;
    }

    static ThreadPool& DefaultPool() {
        return Instance<ShortTimeThreadPool>();
    }
    static ThreadPool& LongTimePool() {
        return Instance<LongTimeThreadPool>();
    }
    static ThreadPool& BlockTimePool() {
        return Instance<BlockTimeThreadPool>();
    }
    static ThreadPool& PrallelTaskPool() {
        return Instance<PrallelThreadPool>();
    }
    static ThreadPool& ProducerPool() {
        return Instance<ProducerThreadPool>();
    }
    static ThreadPool& ConsumerPool() {
        return Instance<ConsumerThreadPool>();
    }
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    std::size_t Count() const;
    std::size_t PendingTasks() const;
    void Spawn(int count = 1);
    /// @brief  join all work threads
    /// @return joined thread nums
    std::size_t Join();
    bool WaitAllTaskFinished() const;

    bool RunOne(std::int64_t idle_wait_time_ms, bool& is_timeout);

    template <typename _Callable, typename... _Args>
    auto Enqueue(_Callable&& f, _Args&&... args) -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {
        return Schedule<true>(clock_t::now(), std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <typename _Callable, typename... _Args>
    auto Schedule(clock_t::duration delay, _Callable&& f, _Args&&... args)
        -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {
        return Schedule<true>(clock_t::now() + delay, std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <bool prepare_worker, typename _Callable, typename... _Args>
    auto Schedule(clock_t::time_point time, _Callable&& f, _Args&&... args)
        -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {

        using ResultType = std::invoke_result_t<_Callable, _Args...>;
        auto promise     = std::make_shared<std::promise<ResultType>>();
        auto bound       = std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...);
        auto task        = [bound, promise]() {
            try {
                if constexpr (std::is_same_v<ResultType, void>) {
                    bound();
                    promise->set_value();
                } else {
                    promise->set_value(bound());
                }
            } catch (const std::exception& e) {
                promise->set_exception(std::make_exception_ptr(e));
            }
        };
        ThreadPoolTaskToken<ResultType> result;
        result.resultFuture = std::move(promise->get_future());
        std::unique_lock<std::mutex> lock(m_tasksMutex);
        m_tasks.push({ time, task, result.status });
        m_condition.notify_one();
        [[maybe_unused]] auto current_size = m_tasks.size();
        lock.unlock();
        if constexpr (prepare_worker) {
            PrepareWorkers(current_size);
        }
        return result;
    }

    void InitThreadPool(const ThreadPoolConfig& config);
    bool InThreadPool();
    ThreadPool(std::int32_t type = ThreadPoolType::ShortTimeThreadPool) : type(type) {
    }
    ~ThreadPool();

protected:
    Task Dequeue(std::int64_t idle_wait_time_ms, bool& is_timeout); // returns null function if joining
    void Run(std::size_t index);
    void PrepareWorkers(std::size_t current_task_nums);

private:
    std::atomic_bool has_alread_configed = false;
    ThreadPoolConfig config_;
    std::size_t spawn_cnt = 0;
    const std::int32_t type;
    std::atomic<std::size_t> waiting_threads = 0;
    std::map<std::size_t, std::unique_ptr<std::thread>> m_workers;
    std::atomic<bool> m_joining { false };

    std::priority_queue<Task, std::deque<Task>, std::greater<Task>> m_tasks;

    mutable std::mutex m_tasksMutex, m_workersMutex;
    mutable std::condition_variable m_condition, m_no_pending_cv;
};

} // namespace Fundamental
