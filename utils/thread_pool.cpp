// ────────────────────────────────────────────
//  File: thread_pool.cpp · Created by Yash Patel · 8-13-2025
// ────────────────────────────────────────────

#include "thread_pool.hpp"

namespace keplar
{
    ThreadPool::ThreadPool(size_t maxThreads) 
        : m_stop(false)
    {
        // fallback to at least one thread
        maxThreads = std::max<size_t>(1, maxThreads);

        // spawn worker threads
        m_workers.reserve(maxThreads);
        for (size_t i = 0; i < maxThreads; ++i)
        {
            m_workers.emplace_back([this]()
            {
                for (;;)
                {
                    {
                        // wait until there's task or pool is stopping
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_taskAvailable.wait(lock, [this]()
                        {
                            return (m_stop || !m_tasks.empty());
                        });

                        // exit if stopped and no task is queued
                        if (m_stop && m_tasks.empty())
                        {
                            return;
                        }

                        // fetch the next task
                        TaskWrapper task = std::move(m_tasks.front());
                        m_tasks.pop();

                        // release mutex and execute the task
                        lock.unlock();
                        task();
                    }

                    // signal waitIdle() that all queued tasks have completed
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        if (m_tasks.empty()) 
                        {
                            m_waitIdle.notify_all();
                        }
                    }
                }
            });
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        // wake up all workers so they can exit
        m_taskAvailable.notify_all();

        // join all worker threads
        for (auto& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void ThreadPool::waitIdle()
    {
        // wait until the queue is empty and all workers are idle
        std::unique_lock<std::mutex> lock(m_mutex);
        m_waitIdle.wait(lock, [this]()
        {
            return m_tasks.empty();
        });
    }
}
