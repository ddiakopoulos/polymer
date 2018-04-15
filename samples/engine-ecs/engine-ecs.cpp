#include "polymer-typeid.hpp"
#include <unordered_map>

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

using namespace polymer;

 //////////////////
 //   Entities   //
 //////////////////

 // An Entity is an uniquely identifiable object in the Polymer runtime.
 using entity = uint64_t;
 constexpr entity kInvalidEntity = 0;

 // Provide a consistent way to retrieve an Entity to which a component belongs. 
 class component 
 {
     entity e;
 public:
     explicit component(entity e = kInvalidEntity) : e(e) {}
     entity get_entity() const { return e; }
 };

 // Hash functor for Components so they can be used in unordered containers. It uses the Component's Entity as the key value.
 struct component_hash 
 {
     entity operator()(const component & c) const { return c.get_entity(); }
 };

 // Systems are responsible for storing the component data instances associated with Entities.
 // They also perform all the logic for manipulating and processing their Components.
 // This base class provides an API for an entity_manager to associate Components with Entities in a data-driven manner.
 struct entity_manager;

 struct base_system : public non_copyable
 {
     entity_manager * factory;

     explicit base_system(entity_manager * f) : factory(f) {}
     virtual ~base_system() {}

     // Associates a default-constructed component with an entity
     virtual bool create(entity e, poly_typeid hash) = 0;

     // Associates component with the Entity using the serialized data
     virtual bool create(entity e, poly_typeid hash, void * data) = 0;

     // Disassociates all component data from the entity in this system
     virtual void destroy(entity e) = 0;

     // Helper function to signal to the entity manager that this system operates on these types of components
     template <typename S>
     void register_system_for_type(S * system, poly_typeid type) { register_system_for_type(get_typeid<S>(), type); }
     void register_system_for_type(poly_typeid system_type, poly_typeid type);
 };

 struct entity_manager
 {
     std::mutex createMutex;

     std::unordered_map<poly_typeid, poly_typeid> system_type_map;
     entity entity_counter { 0 }; // Autoincrementing value to generate unique ids.

     template <typename T, typename... Args>
     T * create_system(Args &&... args);

     void register_system_for_type(poly_typeid system_type, poly_hash_value def_type) 
     {
         system_type_map[def_type] = system_type;
     }

     entity create()
     {
         std::lock_guard<std::mutex> guard(createMutex);
         const entity e = ++entity_counter;
         return e;
     }

     void add_system(poly_typeid system_type, base_system * system)
     {
         if (!system) return;
         auto itr = systems.find(system_type);
         if (itr == systems.end()) systems.emplace(system_type, system);
     }

     std::unordered_map<poly_typeid, base_system *> systems;
 };

 template <typename T, typename... Args>
 T * entity_manager::create_system(Args &&... args) 
 {
     T * ptr = new T(std::forward<Args>(args)...);
     add_system(get_typeid<T>(), ptr);
     return ptr;
 }

 void base_system::register_system_for_type(poly_typeid system_type, poly_typeid type) { factory->register_system_for_type(system_type, type); }

///////////////////
// Serialization //
///////////////////

struct physics_component : public component
{
    physics_component() {};
    physics_component(entity e) : component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(physics_component);

struct render_component : public component
{
    render_component() {}
    render_component(entity e) : component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(render_component);

template <typename T>
std::string serialize_to_json(T e)
{
    std::ostringstream oss;
    {
        cereal::JSONOutputArchive json_archiver(oss);
        json_archiver(e);
    }
    return oss.str();
}

template <typename T>
void deserialize_from_json(const std::string & json_str, T & e)
{
    std::istringstream iss(json_str);
    cereal::JSONInputArchive json_archiver(iss);
    json_archiver(e);
}

template<class F> void visit_fields(render_component & o, F f)
{
    f("v1", o.value1);
    f("v2", o.value2);
    f("v3", o.value3);
}

template<class F> void visit_fields(physics_component & o, F f)
{
    f("v1", o.value1);
    f("v2", o.value2);
    f("v3", o.value3);
}

template<class Archive> void serialize(Archive & archive, render_component & m)
{
    visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
}

template<class Archive> void serialize(Archive & archive, physics_component & m)
{
    visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
}

struct ex_system_one final : public base_system
{
    const poly_typeid c1 = get_typeid<physics_component>();
    ex_system_one(entity_manager * f) : base_system(f) { register_system_for_type(this, c1); }
    ~ex_system_one() override { }
    bool create(entity e, poly_typeid hash) override final 
    { 
        if (c1 != hash) std::cout << "it's an error" << std::endl;
        components[e] = physics_component(e);
        return true; 
    }
    bool create(entity e, poly_typeid hash, void * data) override final 
    { 
        if (hash != c1) { return false; } 
        auto new_component = physics_component(e);
        new_component = *static_cast<physics_component *>(data);
        components[e] = std::move(new_component);
        return true;
    }
    void destroy(entity entity) override final { }
    std::unordered_map<entity, physics_component> components;
};
POLYMER_SETUP_TYPEID(ex_system_one);

struct ex_system_two final : public base_system
{
    const poly_typeid c2 = get_typeid<render_component>();
    ex_system_two(entity_manager * f) : base_system(f) { register_system_for_type(this, c2); }
    ~ex_system_two() override { }
    bool create(entity e, poly_typeid hash) override final 
    { 
        if (c2 != hash) std::cout << "it's an error" << std::endl;
        components[e] = render_component(e);
        return true;
    }

    bool create(entity e, poly_typeid hash, void * data) override final 
    { 
        if (hash != c2) { return false; } 
        auto new_component = render_component(e);
        new_component = *static_cast<render_component *>(data);
        components[e] = std::move(new_component);
        return true; 
    }
    void destroy(entity entity) override final { }
    std::unordered_map<entity, render_component> components;
};
POLYMER_SETUP_TYPEID(ex_system_two);

template<class F> void visit_systems(base_system * s, F f)
{
    f("system_one", dynamic_cast<ex_system_one *>(s));
    f("system_two", dynamic_cast<ex_system_two *>(s));
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    entity_manager factory;

    /*
    render_component c;
    c.value1 = 1.f;
    c.value2 = 2.f;
    c.value3 = 3.f;

    auto str = serialize_to_json(c);

    std::cout << "Serialized: " << str << std::endl;

    render_component ds = {};

    deserialize_from_json(str, ds);

    visit_fields(ds, [&](const char * name, auto & field, auto... metadata) 
    { 
        std::cout << "Name: " << name << ", " << field << std::endl;
    });

    visit_systems(system.second, [&](const char * name, auto * system_pointer)
    {
        if (system_pointer) {}
    });
    */

    render_component s(factory.create());
    s.value1 = 1.f;
    s.value2 = 2.f;
    s.value3 = 3.f;
    auto str = serialize_to_json(s);

    render_component ds = (factory.create());
    deserialize_from_json(str, ds);

    auto sys1 = factory.create_system<ex_system_one>(&factory);
    auto sys2 = factory.create_system<ex_system_two>(&factory);

    auto someEntity = factory.create();
    std::cout << "New Entity is: " << someEntity << std::endl;

    sys2->create(someEntity, get_typeid<render_component>(), &ds);

    for (auto & e : sys2->components)
    {
        std::cout << "Iterate Entity: " << e.first << std::endl;
        visit_fields(e.second, [&](const char * name, auto & field, auto... metadata)
        {
            std::cout << "\tName: " << name << ", " << field << std::endl;
        });
    }

    // Hierarchy aware: transform, collision, events

    /*
     * ++ entity
     *  - component [components];
     *  - children [entity]
     */

    // Get list of all systems
    for (auto & system : factory.systems)
    {

        if (auto * sys = dynamic_cast<ex_system_one *>(system.second))
        {

        }
    }

    // Serialize an entity
    auto serialize_entity = [&](entity e)
    {

    };

    std::this_thread::sleep_for(std::chrono::seconds(100));

    return EXIT_SUCCESS;
}