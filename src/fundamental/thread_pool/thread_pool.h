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
#include <deque>
#include <functional> //bind
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue> //priority_queue
#include <thread>
#include <vector>
namespace Fundamental {
using clock_t = std::chrono::steady_clock;

enum ThreadPoolType : std::int32_t {
    ShortTimeThreadPool = 0,
    LongTimeThreadPool  = 1,
    BlockTimeThreadPool = 2,
    PrallelThreadPool   = 3
};

enum ThreadPoolTaskStatus : std::uint32_t {
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
        ThreadPoolTaskStatus old = status->status.load();
        if (status->status.compare_exchange_strong(old, ThreadTaskCancelled)) {
            return true;
        }
        return false;
    }
    std::shared_ptr<ThreadPoolTaskStatusSyncerWarpper> status = std::make_shared<ThreadPoolTaskStatusSyncerWarpper>();
    std::future<ResultType> resultFuture;
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

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    std::size_t Count() const;
    void Spawn(int count = 1);
    void Join();

    bool RunOne();

    template <typename _Callable, typename... _Args>
    auto Enqueue(_Callable&& f, _Args&&... args) -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {
        return Schedule(clock_t::now(), std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <typename _Callable, typename... _Args>
    auto Schedule(clock_t::duration delay, _Callable&& f, _Args&&... args)
        -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {
        return Schedule(clock_t::now() + delay, std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <typename _Callable, typename... _Args>
    auto Schedule(clock_t::time_point time, _Callable&& f, _Args&&... args)
        -> ThreadPoolTaskToken<std::invoke_result_t<_Callable, _Args...>> {

        using ResultType      = std::invoke_result_t<_Callable, _Args...>;
        auto promise = std::make_shared<std::promise<ResultType>>();
        auto bound   = std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...);
        auto task    = [bound, promise]() {
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
        return result;
    }

    bool InThreadPool();

protected:
    ThreadPool(std::int32_t type = ThreadPoolType::ShortTimeThreadPool) : type(type) {
    }
    ~ThreadPool();

    Task Dequeue(); // returns null function if joining
    void Run(std::size_t index);

private:
    const std::int32_t type;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_joining { false };

    std::priority_queue<Task, std::deque<Task>, std::greater<Task>> m_tasks;

    mutable std::mutex m_tasksMutex, m_workersMutex;
    std::condition_variable m_condition;
};

} // namespace Fundamental
