#pragma once

#ifndef polymer_property_hpp
#define polymer_property_hpp

#include "any.hpp"
#include <functional>

namespace polymer
{

    struct property_action_interface
    {
        virtual void set_value(polymer::any property) = 0;
        virtual polymer::any get_value() = 0;
        virtual ~property_action_interface() {}
    };

    template<typename T>
    class property : public property_action_interface
    {
    public:

        typedef std::function<void(const T & v)> listener_t;

    private:

        std::vector<listener_t> listeners;

        std::function<T(T v)> _set_kernel;
        std::function<T()> _get_kernel;

        mutable T _cached_value;
        mutable bool cache_dirty {true};

        /// @todo - we can also provide the old value alongside the new one
        void notify_listeners()
        {
            for (auto & listener : listeners) 
            {
                listener(_cached_value);
            }
        }

    public:

        property() = default;
        property(const T & default_value) : _cached_value(default_value) {}
        ~property() = default;
        property(const property & r) {} // fixme

        property & operator = (T & other) { _cached_value = other._cached_value; return *this; }

        property & operator = (property && r) noexcept 
        {
            listeners = std::move(r.listeners);
            _set_kernel = std::move(r._set_kernel);
            _get_kernel = std::move(r._get_kernel);
            _cached_value = std::move(_cached_value);
            return *this; 
        }
        property(property && r) { *this = std::move(r); }

        void kernel_set(std::function<T(T v)> set_kernel) { _set_kernel = set_kernel; }
        void kernel_get(std::function<T()> get_kernel) { _get_kernel = get_kernel; }

        void set(const T & new_value)
        {
            if (_set_kernel) { _cached_value = _set_kernel(new_value); }
            else { _cached_value = new_value; }
            notify_listeners();
        }

        T value() const
        {
            if (_get_kernel && cache_dirty) 
            {
                _cached_value = _get_kernel();
                cache_dirty = false;
            }
            return _cached_value;
        }

        T & raw()
        {
            cache_dirty = true;
            return _cached_value;
        }

        // Satisfy `property_action_interface`
        virtual void set_value(polymer::any property) override final
        {
            try
            {
                T new_value = polymer::any_cast<T>(property);
                set(new_value);
            }
            catch (const std::exception & e) { std::cerr << "caught any_cast exception: " << e.what() << std::endl; }
        }

        virtual polymer::any get_value() override final { return polymer::make_any<T>(value()); }

        operator T () const { return value(); }
        bool operator ==(const property & other) { return value() == other.value(); }
        bool operator !=(const property & other) { return value() != other.value(); }

        void add_listener(listener_t l) { listeners.emplace_back(l); }
        void clear_listeners() { listeners.clear(); }
    };

} // end namespace polymer

#endif // end polymer_property_hpp
