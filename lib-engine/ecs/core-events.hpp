#pragma once

#ifndef polymer_core_events_hpp
#define polymer_core_events_hpp

#include "typeid.hpp"
#include "core-ecs.hpp"
#include "mpmc_blocking_queue.hpp"

namespace polymer
{
    ///////////////////////////////////////////
    //   Polymer Core Event Implementation   //
    ///////////////////////////////////////////

    // `event_wrapper` is a modified version of the concept found in Google's Lullaby. 
    // It was changed to only support the notion of a "concrete" event, a compile-time
    // event definition (as separate from a potential future runtime-definable events
    // using a variant type). The wrapper idea persists in Polymer as a point of future extension. 
    // An `event_wrapper` by default does not own an event, however a copy operation 
    // fully copies the underlying event and assumes ownership. 
    class event_wrapper
    {
        enum op : uint32_t
        {
            copy,
            destruct
        };

        enum lifetime : uint32_t
        {
            transient,
            concrete,
        };

        using pointer_fn = void(*)(op, void *, const void *);
        mutable pointer_fn pointer_op{ nullptr };

        template <typename E>
        static void do_pointer_op(op o, void * dst, const void * src)
        {
            switch (o)
            {
            case copy: { const E * other = reinterpret_cast<const E *>(src); new (dst) E(*other); break; }
            case destruct: { E * evt = reinterpret_cast<E*>(dst); evt->~E(); break; }
            }
        }

        mutable poly_typeid type{ 0 };     // typeid of the wrapped event.
        mutable size_t size{ 0 };          // sizeof() the wrapped event.
        mutable size_t align{ 0 };         // alignof() the wrapped event.
        mutable void * data{ nullptr };    // pointer to the wrapped event.
        mutable lifetime life{ transient };

    public:

        event_wrapper() = default;
        ~event_wrapper()
        {
            if (life == lifetime::concrete)
            {
                pointer_op(op::destruct, data, nullptr);
                polymer_aligned_free(data);
            }
        }

        template <typename E>
        explicit event_wrapper(const E & evt)
            : type(get_typeid<E>()), size(sizeof(E)), data(const_cast<E*>(&evt)), align(alignof(E)), pointer_op(&do_pointer_op<E>) { }

        event_wrapper(const event_wrapper & rhs)
            : type(rhs.type), size(rhs.size), align(rhs.align), pointer_op(rhs.pointer_op), life(lifetime::concrete)
        {
            if (rhs.data)
            {
                data = polymer_aligned_alloc(size, align);
                pointer_op(op::copy, data, rhs.data);
            }
        }

        template <typename E>
        const E * get() const
        {
            if (type != get_typeid<E>())  return nullptr;
            return reinterpret_cast<const E*>(data);
        }

        poly_typeid get_type() const { return type; }
    };

    typedef uint32_t connection_id; // unique id per event
    typedef std::function<void(const event_wrapper & evt)> event_handler;

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

    class connection
    {
        poly_typeid type{ 0 };
        connection_id id{ 0 };

        std::weak_ptr<event_handler_map> handlers;
    public:
        connection() {};
        connection(const std::weak_ptr<event_handler_map> & handlers, poly_typeid type, connection_id id) : handlers(handlers), type(type), id(id) {}

        void disconnect()
        {
            if (auto handler = handlers.lock())
            {
                handler->remove(type, id, nullptr);
                handler.reset();
            }
        }
    };

    class scoped_connection
    {
        connection c;
        scoped_connection(const scoped_connection &) = delete;
        scoped_connection & operator= (const scoped_connection &) = delete;
    public:
        scoped_connection(connection c) : c(c) {}
        scoped_connection(scoped_connection && r) : c(r.c) { r.c = {}; }
        scoped_connection & operator= (scoped_connection && r) { if (this != &r) disconnect(); c = r.c; r.c = {}; return *this; }
        ~scoped_connection() { disconnect(); }
        void disconnect() { c.disconnect(); }
    };

    ////////////////////////////////////
    //   Synchronous Event Mananger   //
    ////////////////////////////////////

    // This event mananger tracks connections between types of events
    // and their associated connected handlers. A variety of convenience
    // functions are provided to connect through an event's typeid or
    // by template deduction. As the name suggests, events dispatched
    // through this manager are invoked synchronously on the calling thread;
    // events are processed by their handlers immediately. This type
    // of behavior is useful for some types of system implementations,
    // such as a user interface with a dedicated thread.
    class event_manager_sync
    {
    protected:

        connection_id id{ 0 }; // autoincrementing counter
        std::shared_ptr<event_handler_map> handlers;

        // Function declaration that is used to extract a type from the event handler
        template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &) const);
        template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &));

        connection connect_impl(poly_typeid type, const void * owner, event_handler handler)
        {
            const connection_id new_id = ++id;
            handlers->add(type, new_id, owner, std::move(handler));
            return connection(handlers, type, new_id);
        }

        /* todo - removes the handler that matches the the type and owner */
        void disconnect_impl(poly_typeid type, const void * owner) {}

        virtual bool send_internal(const event_wrapper & event_w)
        {
            return handlers->dispatch(event_w);
        }

    public:

        event_manager_sync() : handlers(std::make_shared<event_handler_map>()) {}

        // This is used by the async event queue
        virtual void process() {};

        // Send an event. Events must be hashable and registered via poly_typeid.
        template <typename E>
        bool send(const E & event)
        {
            return send_internal(event_wrapper(event));
        }

        // A connection that must be manually disconnected. If a non-null
        // owner is specified, the pointer can be used by users of the
        // event handler as an additional way to disconnect beyond calling
        // .disconnect() on the connection object. 
        template <typename Fn>
        connection connect(const void * owner, Fn && fn)
        {
            using function_signature = typename std::remove_reference<Fn>::type;
            using event_t = decltype(connect_helper(&function_signature::operator()));

            return connect_impl(get_typeid<event_t>(), owner, [fn](const event_wrapper & event) mutable
            {
                const event_t * obj = event.get<event_t>();
                fn(*obj); // invoke the foreign handler
            });
        }

        // The type of connection is dependent on the function signature 
        // of Fn (e.g. void(const event_t & e)). It will be disconnected when it 
        // goes out of scope. 
        template <typename Fn>
        scoped_connection connect(Fn && handler) { return connect(nullptr, std::forward<Fn>(handler)); }

        /* todo - connect a handler directly to a type */
        scoped_connection connect(poly_typeid type, event_handler handler) {}

        /* todo - connect a handler to all events that are passed through the dispatcher */
        scoped_connection connect_all(event_handler handler) {}

        /* todo - disconnects all functions listening to an event associated with the following owner. */
        template <typename E>
        void disconnect(const void * owner) {}

        /* todo - disconnects all functions listening to events of the specified type associated with the specified owner */
        void disconnect(poly_typeid type, const void * owner) {}

        /* todo - disconnects all functions with the specified owner */
        void disconnect_all(const void * owner) {}

        // How many functions are currently registered?
        size_t num_handlers() const { return handlers->size(); }

        // How many functions listening for this type of event?
        size_t num_handlers_type(poly_typeid type) const { return handlers->handler_count(type); }
    };

    /////////////////////////////
    //   Async Event Manager   //
    /////////////////////////////

    // This type of event manager queues up events and batches them
    // when users call `process()`. This class is kept simple and does
    // not perform any threading. Notably different from the sync variant,
    // we use an `event_wrapper` to keep copies of the event alive until
    // they are sent and handled (otherwise we'd run into lifetime issues). 
    class event_manager_async : public event_manager_sync
    {
        mpmc_queue_blocking<std::unique_ptr<event_wrapper>> queue;

        event_manager_async(const event_manager_async &) = delete;
        event_manager_async & operator=(const event_manager_async &) = delete;

        virtual bool send_internal(const event_wrapper & event_w) override final
        {
            std::unique_ptr<event_wrapper> event_copy(new event_wrapper(event_w));
            queue.produce(std::move(event_copy));
            return true;
        }

    public:

        event_manager_async() = default;
        ~event_manager_async() = default;

        bool empty() const { return queue.empty(); }

        // Callbacks will happen on the calling thread. It's expected that 
        // this function is only invoked from a single thread, like an an updater.
        virtual void process() override final
        {
            std::unique_ptr<event_wrapper> event;
            while (queue.try_consume(event))
            {
                event_manager_sync::send_internal(*event);
            }
        }
    };

} // end namespace polymer

#endif // end polymer_core_events_hpp

