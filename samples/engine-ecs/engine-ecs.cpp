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

     // The Hash of the actual type used for safely casting the definition to a concrete type for extracting data.
     using DefinitionType = poly_hash_value;

     // Associates a default-constructed component with an entity
     virtual bool create(entity e, DefinitionType component_name_hash) = 0;

     // Associates component with the Entity using the serialized data
     virtual bool create(entity e, DefinitionType component_name_hash, void * data) = 0;

     /* todo - create with default-constructed component */

     // Disassociates all component data from the entity in this system
     virtual void destroy(entity e) = 0;

     // Helper function to signal to the entity manager that this system operates on these types of components
     template <typename S>
     void register_system_for_type(S * system, DefinitionType type) { register_system_for_type(get_typeid<S>(), type); }
     void register_system_for_type(poly_typeid system_type, DefinitionType type);
 };

 struct entity_manager
 {
     std::mutex createMutex;

     using TypeMap = std::unordered_map<system::DefinitionType, poly_typeid>;    // ComponentDef type (hashed) to System TypeId map.
     TypeMap system_type_map;                                               // Map of ComponentDef type (hash) to System TypeIds.
     entity entity_counter { 0 };                                           // Autoincrementing value to generate unique Entity IDs.

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
 void system::register_system_for_type(poly_typeid system_type, DefinitionType type) { factory->register_system_for_type(system_type, type); }

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

IMPLEMENT_MAIN(int argc, char * argv[])
{
    /*
    entity_manager factory;

    // ----

    name_system system(&factory);

    const entity mesh = factory.create();

    system.set_name(mesh, "static_mesh");
    std::cout << "Lookup By Name:   " << system.get_name(mesh) << std::endl;
    std::cout << "Lookup By Entity: " << system.find_entity("static_mesh") << std::endl;

    std::cout << "verify: bool - " << verify_typename<bool>("bool") << std::endl;
    std::cout << "verify: uint64_t - " << verify_typename<uint64_t>("uint64_t") << std::endl;
    std::cout << "verify: float2 - " << verify_typename<float2>("float2") << std::endl;
    std::cout << "verify: Bounds2D - " << verify_typename<aabb_2d>("Bounds2D") << std::endl;

    */
    example_component c;
    c.value1 = 1.f;
    c.value2 = 2.f;
    c.value3 = 3.f;

    std::cout << serialize_to_json(c) << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(100));

    return EXIT_SUCCESS;
}