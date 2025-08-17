// ────────────────────────────────────────────
//  File: thread_pool.hpp · Created by Yash Patel · 8-13-2025
// ────────────────────────────────────────────

#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>

namespace keplar
{
    class TaskWrapper 
    {
        public:            
            // construct from any callable (lambda, functor...) and type-erase it
            template<typename F>
            TaskWrapper(F&& f) : m_callable(std::make_unique<CallableImpl<std::decay_t<F>>>(std::forward<F>(f))) {}

            // move-only semantics
            TaskWrapper(TaskWrapper&&) = default;
            TaskWrapper& operator=(TaskWrapper&&) = default;
            TaskWrapper(const TaskWrapper&) = delete;
            TaskWrapper& operator=(const TaskWrapper&) = delete;

             // invoke the wrapped callable
            void operator()() { (*m_callable)(); }

        private:
             // abstract interface used for type-erasure
            struct CallableBase
            {
                virtual ~CallableBase() = default;
                virtual void operator()() = 0; 
            };

            // concrete implementation storing the actual callable
            template<typename F>
            struct CallableImpl : CallableBase
            {
                CallableImpl(F&& f) : mFunc(std::move(f)) {}
                void operator()() override { mFunc(); }
                F mFunc;
            };

            std::unique_ptr<CallableBase> m_callable;
    };

    class ThreadPool final
    {
        public:
            // creation and destruction
            explicit ThreadPool(size_t maxThreads = std::thread::hardware_concurrency());
            ~ThreadPool();

            // disable copy and move semantics to enforce unique ownership
            ThreadPool(const ThreadPool&) = delete;
            ThreadPool& operator=(const ThreadPool&) = delete;
            ThreadPool(ThreadPool&&) = delete;
            ThreadPool& operator=(ThreadPool&&) = delete; 

            // dispatch the task to a worker thread for async execution
            template<typename F>
            void dispatch(F&& f) 
            {
                {
                    // don’t accept new tasks on stop
                    std::unique_lock<std::mutex> lock(m_mutex);
                    if (m_stop) return;

                    // wrap lambda internally in UniqueTask
                    m_tasks.emplace(TaskWrapper(std::forward<F>(f)));
                }

                // notify a worker that a new task is available
                m_taskAvailable.notify_one();
            }

            // wait until all queued tasks are finished
            void waitIdle();

        private:       
            std::vector<std::thread>            m_workers;
            std::queue<TaskWrapper>             m_tasks;
            std::mutex                          m_mutex;
            std::condition_variable             m_taskAvailable;
            std::condition_variable             m_waitIdle;
            bool                                m_stop;
    };
}
