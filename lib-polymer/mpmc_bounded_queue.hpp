// This is free and unencumbered software released into the public domain.
// Original Source: http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
// Modified to support single-producer as well (spmc)

#ifndef mpmc_bounded_queue_hpp
#define mpmc_bounded_queue_hpp

#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <vector>

template<typename T>
class MPMCBoundedQueue
{
    
    struct node_t { T data; std::atomic<size_t> next; };
    typedef typename std::aligned_storage<sizeof(node_t), std::alignment_of<node_t>::value>::type aligned_node_t;
    typedef char cache_line_pad_t[64];

    cache_line_pad_t pad0;
    const size_t size;
    const size_t mask;
    node_t * const buffer;
    cache_line_pad_t pad1;
    std::atomic<size_t> head{ 0 };
    cache_line_pad_t pad2;
    std::atomic<size_t> tail{ 0 };
    cache_line_pad_t pad3;

    MPMCBoundedQueue(const MPMCBoundedQueue &) { }
    void operator= (const MPMCBoundedQueue &) { }

public:

    MPMCBoundedQueue(size_t size = 1024) : size(size), mask(size-1), buffer(reinterpret_cast<node_t*>(new aligned_node_t[size]))
    {
        assert((size != 0) && ((size & (~size + 1)) == size)); // enforce power of 2
        for (size_t i = 0; i < size; ++i) buffer[i].next.store(i, std::memory_order_relaxed);
    }

    ~MPMCBoundedQueue()
    {
        delete[] buffer;
    }
    
    bool sp_produce(T const & input)
    {
        node_t * node = &buffer[headSequence & mask];
        size_t nodeSequence = node->node.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)nodeSequence - (intptr_t)headSequence;

        if (dif == 0) 
        {
            ++head;
            node->data = input;
            node->next.store(headSequence, std::memory_order_release);
            return true;
        }

        assert(dif < 0);
        return false;
    }

    bool mp_produce(const T & input)
    {
        size_t headSequence = head.load(std::memory_order_relaxed);

        while (true)
        {
            node_t * node = &buffer[headSequence & mask];
            size_t nodeSequence = node->next.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)nodeSequence - (intptr_t)headSequence;

            if (dif == 0)
            {
                if (head.compare_exchange_weak(headSequence, headSequence + 1, std::memory_order_relaxed))
                {
                    node->data = input;
                    node->next.store(headSequence + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (dif < 0)
            {
                return false;
            }
            else
            {
                headSequence = head.load(std::memory_order_relaxed);
            }
        }

        return false;
    }

    bool consume(T & output)
    {
        size_t tailSequence = tail.load(std::memory_order_relaxed);

        while (true)
        {
            node_t * node = &buffer[tailSequence & mask];
            size_t nodeSequence = node->next.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)nodeSequence - (intptr_t)(tailSequence + 1);
            if (dif == 0)
            {
                if (tail.compare_exchange_weak(tailSequence, tailSequence + 1, std::memory_order_relaxed))
                {
                    output = node->data;
                    node->next.store(tailSequence + mask + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (dif < 0)
            {
                return false;
            }
            else
            {
                tailSequence = tail.load(std::memory_order_relaxed);
            }
        }
        return false;
    }

};

#endif // end mpmc_bounded_queue_hpp
