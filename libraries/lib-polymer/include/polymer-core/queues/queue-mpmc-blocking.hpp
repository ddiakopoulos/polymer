// This is free and unencumbered software released into the public domain.
// Inspired by https://www.justsoftwaresolutions.co.uk/threading/implementing-a-thread-safe-queue-using-condition-variables.html

#ifndef mpmc_blocking_queue_hpp
#define mpmc_blocking_queue_hpp

#include <mutex>
#include <queue>
#include <condition_variable>

namespace polymer
{
    template<typename T>
    class mpmc_queue_blocking
    {
        std::queue<T> queue;
        mutable std::mutex mutex;
        std::condition_variable condition;

        mpmc_queue_blocking(const mpmc_queue_blocking &) = delete;
        mpmc_queue_blocking & operator= (const mpmc_queue_blocking &) = delete;

    public:

        mpmc_queue_blocking() = default;

        // Produce a new value and possibily notify one of the threads calling `wait_and_consume`
        void produce(T & value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(std::move(value));
            condition.notify_one();
        }

        // Blocking operation if queue is empty
        void wait_and_consume(T & popped_value)
        {
            std::unique_lock<std::mutex> lock(mutex);
            while (queue.empty()) condition.wait(lock);
            popped_value = std::move(queue.front());
            queue.pop();
        }

        // Permits polling threads to do something else if the queue is empty
        bool try_consume(T & popped_value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) return false;
            popped_value = std::move(queue.front());
            queue.pop();
            return true;
        }

        // Is the queue empty? 
        bool empty() const
        {
            std::unique_lock<std::mutex> lock(mutex);
            return queue.empty();
        }
        
        std::size_t size() const { return queue.size(); }
    };

} // end namespace polymer

#endif // end mpmc_blocking_queue_hpp
