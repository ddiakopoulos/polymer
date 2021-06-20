// This is free and unencumbered software released into the public domain.

#ifndef mpsc_bounded_queue_hpp
#define mpsc_bounded_queue_hpp

#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace polymer
{

    // logic inspired by https://github.com/dbittman/waitfree-mpsc-queue/blob/master/mpsc.c
    // https://github.com/meshula/LabCmd/blob/master/include/LabCmd/Queue.h
    template <typename T, size_t N>
    class mpsc_queue_bounded
    {
        std::array<T, N> buffer;
        std::array<std::atomic<T*>, N> ready_buffer;

        std::atomic<size_t> count{ 0 };
        std::atomic<size_t> head{ 0 };
        size_t tail{ 0 };

    public:

        mpsc_queue_bounded() = default;

        // Thread safe
        bool emplace_back(T && val)
        {
            size_t count = count.fetch_add(1, std::memory_order_acquire);
            if (count >= buffer.size())
            {
                count.fetch_sub(1, std::memory_order_release);
                return false; // queue is full
            }

            // Exclusive access to head, relying on unsigned int wrap around to keep head increment atomic
            size_t h = head.fetch_add(1, std::memory_order_acquire) % buffer.size();
            buffer[h] = val;

            // Using a pointer to the element as a flag that the value is consumable
            ready_buffer[h].store(&buffer[h], std::memory_order_release);
            return true;
        }

        bool emplace_back(T const & val)
        {
            size_t cnt = count.fetch_add(1, std::memory_order_acquire);
            if (cnt >= buffer.size())
            {
                count.fetch_sub(1, std::memory_order_release);
                return false; // queue is full
            }

            // get exclusive access to head
            // relying on unsigned int wrap around to keep head increment atomic
            size_t h = head.fetch_add(1, std::memory_order_acquire) % buffer.size();
            buffer[h] = val;

            // using a pointer to the element as a flag that the value is consumable
            ready_buffer[h].store(&buffer[h], std::memory_order_release);
            return true;
        }

        // Safe from one thread only
        std::pair<bool, T> pop_front()
        {
            T * result = ready_buffer[tail].exchange(nullptr, std::memory_order_acquire);

            // result will be null if `emplace_back` is writing, or if the queue is empty
            if (!result) return { false, T{} };

            tail = (tail + 1) % buffer.size();
            const size_t cnt = count.fetch_sub(1, std::memory_order_acquire);

            return { true, *result };
        }

        size_t size() const { return count; }

        bool empty() const { return !count; }
    };

} // end namespace polymer

#endif // mpsc_bounded_queue_hpp
