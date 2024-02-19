/*
 * Based on: https://github.com/google/lullaby/tree/master/lullaby/modules/dispatcher
 * Apache 2.0 License. Copyright 2017 Google Inc. All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#include "polymer-engine/ecs/core-events.hpp"
#include <unordered_map>

namespace polymer
{
    //////////////////////////////////////
    //   Event Wrapper Implementation   //
    //////////////////////////////////////

    event_wrapper::~event_wrapper()
    {
        if (life == lifetime::concrete)
        {
            pointer_op(op::destruct, data, nullptr);
            polymer_aligned_free(data);
        }
    }

    event_wrapper::event_wrapper(const event_wrapper & rhs)
        : type(rhs.type), size(rhs.size), align(rhs.align), pointer_op(rhs.pointer_op), life(lifetime::concrete)
    {
        if (rhs.data)
        {
            data = polymer_aligned_alloc(size, align);
            pointer_op(op::copy, data, rhs.data);
        }
    }

    poly_typeid event_wrapper::get_type() const
    {
        return type;
    }

    //////////////////////////////////////////
    //   Event Handler Map Implementation   //
    //////////////////////////////////////////

    // The event manager uses this as an internal utility to map events
    // to their handlers via `poly_typeid`. It is not currently thread-safe. 
    class event_handler_map
    {
        struct tagged_event_handler
        {
            tagged_event_handler(connection_id id, const void * owner, event_handler fn) : id(id), owner(owner), fn(std::move(fn)) {}
            connection_id id;
            const void * owner;
            event_handler fn;
        };

        int dispatch_count{ 0 };
        std::vector<std::pair<poly_typeid, tagged_event_handler>> command_queue;
        std::unordered_multimap<poly_typeid, tagged_event_handler> map;

        void remove_impl(poly_typeid type, tagged_event_handler handler)
        {
            assert(handler.fn == nullptr);
            assert(handler.id != 0 || handler.owner != nullptr);

            auto range = std::make_pair(map.begin(), map.end());
            if (type != 0) range = map.equal_range(type);

            if (handler.id)
            {
                for (auto it = range.first; it != range.second; ++it)
                {
                    if (it->second.id == handler.id) map.erase(it); return;
                }
            }
            else if (handler.owner)
            {
                for (auto it = range.first; it != range.second;)
                {
                    if (it->second.owner == handler.owner) it = map.erase(it);
                    else ++it;
                }
            }
        }

    public:

        void add(poly_typeid type, connection_id id, const void * owner, event_handler fn)
        {
            tagged_event_handler handler(id, owner, std::move(fn));
            if (dispatch_count > 0) command_queue.emplace_back(type, std::move(handler));
            else
            {
                assert(handler.id != 0);
                assert(handler.fn != nullptr);
                map.emplace(type, std::move(handler));
            }
        }

        void remove(poly_typeid type, connection_id id, const void * owner)
        {
            tagged_event_handler handler(id, owner, nullptr);
            if (dispatch_count > 0) command_queue.emplace_back(type, std::move(handler));
            else remove_impl(type, std::move(handler));
        }

        bool dispatch(const event_wrapper & event)
        {
            size_t handled = 0;

            const poly_typeid type = event.get_type();

            // Dispatches to handlers only matching type (typical case)
            ++dispatch_count;
            auto range = map.equal_range(type);
            for (auto it = range.first; it != range.second; ++it) { it->second.fn(event); handled++; };

            // Dispatches to handlers listening to all events, regardless of type (infrequent)
            range = map.equal_range(0);
            for (auto it = range.first; it != range.second; ++it) { it->second.fn(event); handled++; };
            --dispatch_count;

            if (dispatch_count == 0)
            {
                for (auto & cmd : command_queue)
                {
                    if (cmd.second.fn) map.emplace(cmd.first, std::move(cmd.second)); // queue a remove operation
                    else remove_impl(cmd.first, std::move(cmd.second)); // execute remove
                }
                command_queue.clear();
            }

            return (handled >= 1);
        }

        size_t size() const { return map.size(); }
        size_t handler_count(poly_typeid type) { return map.count(type); }
    };

    ///////////////////////////////////////////
    //   event_manager_sync implementation   //
    ///////////////////////////////////////////

    event_manager_sync::event_manager_sync() : handlers(std::make_shared<event_handler_map>()) {}
    event_manager_sync::~event_manager_sync() {}

    event_manager_sync::connection event_manager_sync::connect_impl(poly_typeid type, const void * owner, event_handler handler)
    {
        const connection_id new_id = ++id;
        handlers->add(type, new_id, owner, std::move(handler));
        return connection(handlers, type, new_id);
    }

    void event_manager_sync::disconnect_impl(poly_typeid type, const void * owner) { /* todo */ }

    bool event_manager_sync::send_internal(const event_wrapper & event_w)
    {
        return handlers->dispatch(event_w);
    }

    void event_manager_sync::connection::disconnect()
    {
        if (auto handler = handlers.lock())
        {
            handler->remove(type, id, nullptr);
            handler.reset();
        }
    }

    event_manager_sync::scoped_connection event_manager_sync::connect(poly_typeid type, event_handler handler) { /* todo */  return {{}}; }

    event_manager_sync::scoped_connection event_manager_sync::connect_all(event_handler handler) { /* todo */ return {{}}; }

    void event_manager_sync::disconnect(poly_typeid type, const void * owner) { /* todo */ }

    void event_manager_sync::disconnect_all(const void * owner) { /* todo */ }

    size_t event_manager_sync::num_handlers() const { return handlers->size(); }

    size_t event_manager_sync::num_handlers_type(poly_typeid type) const { return handlers->handler_count(type); }

    ////////////////////////////////////////////
    //   event_manager_async implementation   //
    ////////////////////////////////////////////

    bool event_manager_async::send_internal(const event_wrapper & event_w)
    {
        std::unique_ptr<event_wrapper> event_copy(new event_wrapper(event_w));
        queue.produce(std::move(event_copy));
        return true;
    }

    bool event_manager_async::empty() const
    {
        return queue.empty();
    }

    void event_manager_async::process()
    {
        std::unique_ptr<event_wrapper> event;
        while (queue.try_consume(event))
        {
            event_manager_sync::send_internal(*event);
        }
    }

} // end namespace polymer
