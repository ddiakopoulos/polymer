// A cleaned up version of https://github.com/progschj/ThreadPool (BSD 3-Clause)

#pragma once

#ifndef polymer_thread_pool_hpp
#define polymer_thread_pool_hpp

#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>
#include <vector>

namespace polymer
{
    class simple_thread_pool
    {
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        std::mutex queue_mutex;
        std::condition_variable cv;
        std::atomic<bool> should_stop{ false };

    public:

        simple_thread_pool(const size_t num_threads = std::thread::hardware_concurrency())
        {
            for (size_t i = 0; i < num_threads; ++i)
            {
                workers.emplace_back([this] {
                    while (true)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lk(queue_mutex);
                            cv.wait(lk, [this] { return should_stop || !tasks.empty(); });
                            if (should_stop && tasks.empty()) return;
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();
                    }
                });
            }
        }

        ~simple_thread_pool()
        {
            should_stop = true;
            cv.notify_all();
            for (std::thread & worker : workers) if (worker.joinable()) worker.join();
        }

        template<class F, class... Args>
        decltype(auto) enqueue(F && f, Args &&... args)
        {
            using return_type = typename std::result_of<F(Args...)>::type;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<return_type> task_future = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (should_stop)  throw std::runtime_error("enqueue on a thread_pool scheduled to exit...");
                tasks.emplace([task]() { (*task)(); });
            }

            cv.notify_one();
            return task_future;
        }
    };

}

#endif
