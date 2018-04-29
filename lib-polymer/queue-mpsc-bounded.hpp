// This is free and unencumbered software released into the public domain.

#ifndef mpsc_bounded_queue_hpp
#define mpsc_bounded_queue_hpp

#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace polymer
{
    template<typename T>
    class mpsc_queue_bounded
    {

        struct buffer_node_t { T data; std::atomic<buffer_node_t*> next; };
        typedef typename std::aligned_storage<sizeof(buffer_node_t), std::alignment_of<buffer_node_t>::value>::type buffer_node_aligned_t;
        std::atomic<buffer_node_t*> head;
        std::atomic<buffer_node_t*> tail;
        mpsc_queue_bounded(const mpsc_queue_bounded &) {}
        void operator= (const mpsc_queue_bounded &) {}

    public:

        mpsc_queue_bounded() : head(reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t)), tail(head.load(std::memory_order_relaxed))
        {
            buffer_node_t * front = head.load(std::memory_order_relaxed);
            front->next.store(nullptr, std::memory_order_relaxed);
        }

        ~mpsc_queue_bounded()
        {
            T output;
            while (this->consume(output)) {}
            buffer_node_t* front = head.load(std::memory_order_relaxed);
            delete front;
        }

        bool produce(const T & input)
        {
            buffer_node_t* node = reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t);
            node->data = input;
            node->next.store(nullptr, std::memory_order_relaxed);

            buffer_node_t* prevhead = head.exchange(node, std::memory_order_acq_rel);
            prevhead->next.store(node, std::memory_order_release);
            return true;
        }

        bool consume(T & output)
        {
            buffer_node_t * tail = tail.load(std::memory_order_relaxed);
            buffer_node_t * next = tail->next.load(std::memory_order_acquire);
            if (next == nullptr) return false;
            output = next->data;
            tail.store(next, std::memory_order_release);
            delete tail;
            return true;
        }

        bool available()
        {
            buffer_node_t * tail = tail.load(std::memory_order_relaxed);
            buffer_node_t * next = tail->next.load(std::memory_order_acquire);
            return next != nullptr;
        }

    };

} // end namespace polymer

#endif // mpsc_bounded_queue_hpp
