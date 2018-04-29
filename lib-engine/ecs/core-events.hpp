#pragma once

#ifndef polymer_core_events_hpp
#define polymer_core_events_hpp

#include "typeid.hpp"
#include "core-ecs.hpp"
#include "queue-mpmc-blocking.hpp"

namespace polymer
{
    ///////////////////////////////////////////
    //   Polymer Core Event Implementation   //
    ///////////////////////////////////////////

    /// `event_wrapper` is a modified version of the concept found in Google's Lullaby. 
    /// It was changed to only support the notion of a "concrete" event, a compile-time
    /// event definition (as separate from a potential future runtime-definable events
    /// using a variant type). The wrapper idea persists in Polymer as a point of future extension. 
    /// An `event_wrapper` by default does not own an event, however a copy operation 
    /// fully copies the underlying event and assumes ownership. 
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

        mutable poly_typeid type{ 0 };      // typeid of the wrapped event.
        mutable size_t size{ 0 };           // sizeof() the wrapped event.
        mutable size_t align{ 0 };          // alignof() the wrapped event.
        mutable void * data{ nullptr };     // pointer to the wrapped event.
        mutable lifetime life{ transient }; // ownership of the wrapped event

    public:

        event_wrapper() = default;
        ~event_wrapper();

        template <typename E>
        explicit event_wrapper(const E & evt)
            : type(get_typeid<E>()), size(sizeof(E)), data(const_cast<E*>(&evt)), align(alignof(E)), pointer_op(&do_pointer_op<E>) { }

        event_wrapper(const event_wrapper & rhs);

        poly_typeid get_type() const;

        template <typename E>
        const E * get() const
        {
            if (type != get_typeid<E>())  return nullptr;
            return reinterpret_cast<const E*>(data);
        }
    };

    typedef uint32_t connection_id; // unique id per event
    typedef std::function<void(const event_wrapper & evt)> event_handler;

    ////////////////////////////////////
    //   Synchronous Event Mananger   //
    ////////////////////////////////////

    // Forward declaration
    class event_handler_map;

    /// This event mananger tracks connections between types of events
    /// and their associated connected handlers. A variety of convenience
    /// functions are provided to connect through an event's typeid or
    /// by template deduction. As the name suggests, events dispatched
    /// through this manager are invoked synchronously on the calling thread;
    /// events are processed by their handlers immediately. This type
    /// of behavior is useful for some types of system implementations,
    /// such as a user interface with a dedicated thread.
    class event_manager_sync
    {

    public:

        class connection
        {
            poly_typeid type{ 0 };
            connection_id id{ 0 };
            std::weak_ptr<event_handler_map> handlers;
        public:
            connection() {};
            connection(const std::weak_ptr<event_handler_map> & handlers, poly_typeid type, connection_id id) : handlers(handlers), type(type), id(id) {}
            void disconnect();
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

    protected:

        connection_id id{ 0 }; // autoincrementing counter
        std::shared_ptr<event_handler_map> handlers;

        // Function declaration that is used to extract a type from the event handler
        template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &) const);
        template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &));

        connection connect_impl(poly_typeid type, const void * owner, event_handler handler);

        /* todo - removes the handler that matches the the type and owner */
        void disconnect_impl(poly_typeid type, const void * owner);

        virtual bool send_internal(const event_wrapper & event_w);

    public:

        event_manager_sync();
        ~event_manager_sync();

        // This is used by the async event queue
        virtual void process() { };

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

        /* todo - disconnects all functions listening to an event associated with the following owner. */
        template <typename E>
        void disconnect(const void * owner) { }

        /* todo - connect a handler directly to a type */
        scoped_connection connect(poly_typeid type, event_handler handler);

        /* todo - connect a handler to all events that are passed through the dispatcher */
        scoped_connection connect_all(event_handler handler);

        /* todo - disconnects all functions listening to events of the specified type associated with the specified owner */
        void disconnect(poly_typeid type, const void * owner);

        /* todo - disconnects all functions with the specified owner */
        void disconnect_all(const void * owner);

        // How many functions are currently registered?
        size_t num_handlers() const;

        // How many functions listening for this type of event?
        size_t num_handlers_type(poly_typeid type) const;
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

        virtual bool send_internal(const event_wrapper & event_w) override final;

    public:

        event_manager_async() = default;
        ~event_manager_async() = default;

        // Callbacks will happen on the calling thread. It's expected that 
        // this function is only invoked from a single thread, like an update().
        virtual void process() override final;

        bool empty() const;
    };

} // end namespace polymer

#endif // end polymer_core_events_hpp

