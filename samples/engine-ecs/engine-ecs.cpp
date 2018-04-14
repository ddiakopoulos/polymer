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
     entity e{ kInvalidEntity };
 public:
     explicit component(entity e) : e(e) {}
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
 struct system : public non_copyable
 {
     entity_manager * factory;

     explicit system(entity_manager * f) : factory(f) {}
     virtual ~system() { }

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

     using TypeMap = std::unordered_map<poly_typeid, poly_typeid>;   // ComponentDef type (hashed) to System TypeId map.
     TypeMap system_type_map;                                                   // Map of ComponentDef type (hash) to System TypeIds.
     entity entity_counter { 0 };                                               // Autoincrementing value to generate unique ids.

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
 };

 // Associates the System with the DefType in the entity_manager.
 void system::register_system_for_type(poly_typeid system_type, poly_typeid type) { factory->register_system_for_type(system_type, type); }

 template <typename T>
 bool verify_typename(const char * name) 
 {
     return type_name_generator::generate<T>() == std::string(name);
 }

///////////////////
// Serialization //
///////////////////

struct example_component
{
    float value1;
    float value2;
    float value3;
};
POLYMER_SETUP_TYPEID(example_component);

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

template<class F> void visit_fields(example_component & o, F f)
{
    f("v1", o.value1);
    f("v2", o.value2);
    f("v3", o.value3);
}

template<class Archive> void serialize(Archive & archive, example_component & m)
{
    visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
};

struct ex_system_one final : public system
{
    const poly_typeid c1 = const_hash_fnv1a("physics_component");
    ex_system_one(entity_manager * f) : system(f) { register_system_for_type(this, c1); }
    ~ex_system_one() override { }
    bool create(entity e, poly_typeid hash) override final { return false; }
    bool create(entity entity, poly_typeid hash, void * data) override final { if (hash != c1) { return false; } return true; }
    void destroy(entity entity) override final { }
};
POLYMER_SETUP_TYPEID(ex_system_one);

struct ex_system_two final : public system
{
    const poly_typeid c2 = get_typeid<example_component>();
    ex_system_two(entity_manager * f) : system(f) { register_system_for_type(this, c2); }
    ~ex_system_two() override { }
    bool create(entity e, poly_typeid hash) override final { return false; }
    bool create(entity entity, poly_typeid hash, void * data) override final { if (hash != c2) { return false; } return true; }
    void destroy(entity entity) override final { }
    std::unordered_map<entity, example_component> components;
};
POLYMER_SETUP_TYPEID(ex_system_two);

IMPLEMENT_MAIN(int argc, char * argv[])
{
    entity_manager factory;

    example_component c;
    c.value1 = 1.f;
    c.value2 = 2.f;
    c.value3 = 3.f;

    auto str = serialize_to_json(c);

    std::cout << "Serialized: " << str << std::endl;

    example_component ds = {};

    deserialize_from_json(str, ds);

    visit_fields(ds, [&](const char * name, auto & field, auto... metadata) 
    { 
        std::cout << "Name: " << name << ", " << field << std::endl;
    });

    ex_system_one system(&factory);

    std::this_thread::sleep_for(std::chrono::seconds(100));

    return EXIT_SUCCESS;
}