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
#include "thread_pool.h"
#include "fundamental/basic/utils.hpp"
namespace Fundamental
{

ThreadPool::~ThreadPool()
{
    Join();
}

int ThreadPool::Count() const
{
    std::unique_lock<std::mutex> lock(m_workersMutex);
    return int(m_workers.size());
}

void ThreadPool::Spawn(int count)
{
    std::unique_lock<std::mutex> lock(m_workersMutex);
    m_joining = false;
    while (count-- > 0)
        m_workers.emplace_back(std::bind(&ThreadPool::Run, this));
}

void ThreadPool::Join()
{
    std::unique_lock<std::mutex> lock(m_workersMutex);
    m_joining = true;
    m_condition.notify_all();

    for (auto& w : m_workers)
        w.join();

    m_workers.clear();
}

void ThreadPool::Run()
{
    Fundamental::Utils::SetThreadName("thp");
    while (RunOne())
    {
    }
}

bool ThreadPool::RunOne()
{
    auto task = Dequeue();
    if (task.func)
    {
        if (task.status->status.load() == ThreadPoolTaskStatus::ThreadTaskCancelled)
            return true;
        task.status->status.exchange(ThreadPoolTaskStatus::ThreadTaskRunning);
        task.func();
        task.status->status.exchange(ThreadPoolTaskStatus::ThreadTaskDone);
        return true;
    }
    return false;
}

ThreadPool::Task ThreadPool::Dequeue()
{
    std::unique_lock<std::mutex> lock(m_tasksMutex);
    while (true)
    {
        if (!m_tasks.empty())
        {
            if (m_tasks.top().time <= clock_t::now())
            {
                ThreadPool::Task task = m_tasks.top();
                m_tasks.pop();
                return task;
            }

            if (m_joining)
                break;

            m_condition.wait_until(lock, m_tasks.top().time);
        }
        else
        {
            if (m_joining)
                break;

            m_condition.wait(lock);
        }
    }
    return ThreadPool::Task();
}
} // namespace RealiNative
