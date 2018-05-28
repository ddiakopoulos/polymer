/*
 * Based on: https://github.com/google/lullaby/tree/master/lullaby/modules/ecs
 * Apache 2.0 License. Copyright 2017 Google Inc. All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#pragma once

#ifndef polymer_base_ecs_hpp
#define polymer_base_ecs_hpp

#include <unordered_map>
#include <limits>
#include "typeid.hpp"

namespace polymer
{
    ////////////////
    //   Entity   //
    ////////////////

    // An entity is an uniquely identifiable object in the Polymer runtime.
    using entity = uint64_t;
    constexpr entity kInvalidEntity = 0;
    constexpr entity kAllEntities = std::numeric_limits<uint64_t>::max();

    ////////////////////////
    //   Base Component   //
    ////////////////////////

    // Provide a consistent way to retrieve an entity to which a component belongs. 
    class base_component
    {
        entity e;
        friend struct component_hash;
        friend class environment; // for serialization to modify e directly
    public:
        explicit base_component(entity e = kInvalidEntity) : e(e) { }
        entity get_entity() const { return e; }
    };

    // Hash functor for components so they can be used in unordered containers. 
    struct component_hash { entity operator()(const base_component & c) const { return c.e; } };

    /////////////////////
    //   Base System   //
    /////////////////////

    // Systems are responsible for storing the component data instances associated with entities.
    // They also perform all the logic for manipulating and processing their Components.
    // This base class provides an API for an entity_orchestrator to associate components with entities in a data-driven manner.

    class entity_orchestrator;
    struct base_system : public non_copyable
    {
        entity_orchestrator * orchestrator;

        explicit base_system(entity_orchestrator * o) : orchestrator(o) {}
        virtual ~base_system() {}

        // Associates component with the Entity using serialized data. The void pointer 
        // and hash type is to subvert the need for a heavily templated component system. 
        virtual bool create(entity e, poly_typeid hash, void * data) = 0;

        // Destroys all of an entity's associated components
        virtual void destroy(entity e) = 0;

        // Helper function to signal to the entity manager that this system operates on these types of components
        template <typename S>
        void register_system_for_type(S * system, poly_typeid component_type) { register_system_for_type(get_typeid<S>(), component_type); }
        void register_system_for_type(poly_typeid system_type, poly_typeid component_type);
    };

    /////////////////////////////
    //   Entity Orchestrator   //
    /////////////////////////////

    class entity_orchestrator
    {
        std::mutex createMutex;
        std::unordered_map<poly_typeid, poly_typeid> system_type_map; // system-to-component-type
        std::unordered_map<poly_typeid, std::unique_ptr<base_system>> systems;
        entity entity_counter{ 0 }; // an autoincrementing value to generate unique ids

    public:

        template <typename T, typename... Args>
        T * create_system(Args &&... args)
        {
            std::unique_ptr<T> ptr(new T(std::forward<Args>(args)...));
            T * return_ptr = ptr.get();
            add_system(get_typeid<T>(), std::move(ptr));
            return return_ptr;
        }

        void register_system_for_type(const poly_typeid system_type, poly_typeid component_type)
        {
            system_type_map[component_type] = system_type;
        }

        entity create_entity()
        {
            std::lock_guard<std::mutex> guard(createMutex);
            const entity e = ++entity_counter;
            return e;
        }

        void add_system(const poly_typeid system_type, std::unique_ptr<base_system> system)
        {
            if (!system) return;
            auto itr = systems.find(system_type);
            if (itr == systems.end()) systems.emplace(system_type, std::move(system)); // new
            else itr->second = std::move(system); // replace
        }

        base_system * get_system(const poly_typeid system_type)
        {
            auto itr = systems.find(system_type);
            if (itr != systems.end()) return itr->second.get();
            else return nullptr;
        }
    };

    inline void base_system::register_system_for_type(poly_typeid system_type, poly_typeid component_type) 
    { 
        orchestrator->register_system_for_type(system_type, component_type);
    }

} // end namespace polymer

#endif // polymer_base_ecs_hpp
