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
namespace Fundamental
{
template <typename _Callable, typename... _Args>
using invoke_future_t = std::future<std::invoke_result_t<_Callable, _Args...>>;
using clock_t         = std::chrono::steady_clock;

enum ThreadPoolType : std::int32_t
{
    ShortTimeThreadPool = -1,
    LongTimeThreadPool  = 0,
    BlockTimeThreadPool = 1,
};

enum ThreadPoolTaskStatus : std::uint32_t
{
    ThreadTaskWaitting,
    ThreadTaskRunning,
    ThreadTaskDone,
    ThreadTaskCancelled
};

struct ThreadPoolTaskStatusSyncerWarpper
{
    std::atomic<ThreadPoolTaskStatus> status = ThreadTaskWaitting;
};

template <typename _Callable, typename... _Args>
struct ThreadPoolTaskToken
{
    bool CancelTask()
    {
        if (status && status->status.load() == ThreadTaskWaitting)
        {
            status->status.exchange(ThreadTaskCancelled);
            return true;
        }
        return false;
    }
    std::shared_ptr<ThreadPoolTaskStatusSyncerWarpper> status = std::make_shared<ThreadPoolTaskStatusSyncerWarpper>();
    invoke_future_t<_Callable, _Args...> resultFuture;
};

class ThreadPool final
{
    struct Task
    {
        clock_t::time_point time;
        std::function<void()> func = nullptr;
        std::shared_ptr<ThreadPoolTaskStatusSyncerWarpper> status;
        bool operator>(const Task& other) const
        {
            return time > other.time;
        }
        bool operator<(const Task& other) const
        {
            return time < other.time;
        }
    };

public:
    template <std::int32_t Index = ShortTimeThreadPool>
    static ThreadPool& Instance()
    {
        // Init handles joining on cleanup
        static ThreadPool* instance = new ThreadPool;
        return *instance;
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

     int Count() const;
     void Spawn(int count = 1);
     void Join();
     void Run();
     bool RunOne();

    template <typename _Callable, typename... _Args>
    auto Enqueue(_Callable&& f, _Args&&... args) -> ThreadPoolTaskToken<_Callable, _Args...>
    {
        return Schedule(clock_t::now(), std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <typename _Callable, typename... _Args>
    auto Schedule(clock_t::duration delay, _Callable&& f, _Args&&... args) -> ThreadPoolTaskToken<_Callable, _Args...>
    {
        return Schedule(clock_t::now() + delay, std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template <typename _Callable, typename... _Args>
    auto Schedule(clock_t::time_point time, _Callable&& f, _Args&&... args) -> ThreadPoolTaskToken<_Callable, _Args...>
    {
        std::unique_lock<std::mutex> lock(m_tasksMutex);
        using R    = std::invoke_result_t<_Callable, _Args...>;
        auto bound = std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...);
        auto task  = std::make_shared<std::packaged_task<R()>>([bound]() {
            try
            {

                return bound();
            }
            catch (const std::exception& e)
            {
                std::cerr << "ThreadPool Run ERROR " << e.what() << std::endl;
                throw;
            }
        });
        ThreadPoolTaskToken<_Callable, _Args...> result;
        result.resultFuture = task->get_future();
        m_tasks.push({ time, [task]() { return (*task)(); }, result.status });
        m_condition.notify_one();
        return result;
    }

protected:
     ThreadPool() = default;
     ~ThreadPool();

     Task Dequeue(); // returns null function if joining

private:
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_joining { false };

    std::priority_queue<Task, std::deque<Task>, std::greater<Task>> m_tasks;

    mutable std::mutex m_tasksMutex, m_workersMutex;
    std::condition_variable m_condition;
};

} // namespace Fundamental
