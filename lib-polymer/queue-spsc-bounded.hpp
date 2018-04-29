// This is free and unencumbered software released into the public domain.

#ifndef spsc_bounded_queue_hpp
#define spsc_bounded_queue_hpp

#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace polymer
{
    template<typename T>
    class spsc_queue_bounded
    {

        typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type aligned_t;
        typedef char cache_line_pad_t[64];

        cache_line_pad_t pad0;
        const size_t size;
        const size_t mask;
        T * const buffer;
        cache_line_pad_t pad1;
        std::atomic<size_t> head{ 0 };
        cache_line_pad_t pad2;
        std::atomic<size_t> tail{ 0 };

        spsc_queue_bounded(const spsc_queue_bounded &) {}
        void operator= (const spsc_queue_bounded &) {}

    public:

        spsc_queue_bounded(size_t size = 1024) : size(size), mask(size - 1), buffer(reinterpret_cast<T*>(new aligned_t[size + 1]))
        {
            assert((size != 0) && ((size & (~size + 1)) == size));
        }

        ~spsc_queue_bounded()
        {
            delete[] buffer;
        }

        bool produce(T & input)
        {
            const size_t h = head.load(std::memory_order_relaxed);

            if (((tail.load(std::memory_order_acquire) - (h + 1)) & mask) >= 1)
            {
                buffer[h & mask] = input;
                head.store(h + 1, std::memory_order_release);
                return true;
            }
            return false;
        }

        bool consume(T & output)
        {
            const size_t t = tail.load(std::memory_order_relaxed);

            if (((head.load(std::memory_order_acquire) - t) & mask) >= 1)
            {
                output = buffer[tail & mask];
                tail.store(t + 1, std::memory_order_release);
                return true;
            }
            return false;
        }

    };

} // end namespace polymer

#endif // spsc_bounded_queue_hpp
