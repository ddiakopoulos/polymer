// This is free and unencumbered software released into the public domain.

#ifndef spsc_queue_hpp
#define spsc_queue_hpp

#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <vector>

template<typename T>
class SPSCQueue
{

    struct node_t { node_t * next; T data; };
    typedef typename std::aligned_storage<sizeof(node_t), std::alignment_of<node_t>::value>::type node_aligned_t;

    node_t * head;
    char cache_line_pad[64];
    node_t * tail;
    node_t * back;

    SPSCQueue(const SPSCQueue &) { }
    void operator= (const SPSCQueue &) { }

public:

    SPSCQueue() : head(reinterpret_cast<node_t*>(new node_aligned_t)), tail(head)
    {
        head->next = nullptr;
    }

    ~SPSCQueue()
    {
        T output;
        while (this->consume(output)) {}
        delete head;
    }

    bool produce(const T & input)
    {
        node_t * node = reinterpret_cast<node_t*>(new node_aligned_t);
        node->data = input;
        node->next = nullptr;

        std::atomic_thread_fence(std::memory_order_acq_rel);
        head->next = node;
        head = node;
        return true;
    }

    bool consume(T & output)
    {
        std::atomic_thread_fence(std::memory_order_consume);
        if (!tail->next) return false;
        output = tail->next->data;
        std::atomic_thread_fence(std::memory_order_acq_rel);
        back = tail;
        tail = back->next;
        delete back;
        return true;
    }
    
};

#endif // spsc_queue_hpp
