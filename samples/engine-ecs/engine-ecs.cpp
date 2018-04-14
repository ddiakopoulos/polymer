#include "polymer-typeid.hpp"
#include <unordered_map>

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
     entity ent{ kInvalidEntity };
 public:
     explicit component(entity e) : ent(e) {}
     entity get_entity() const { return ent; }
 };

 // Hash functor for Components so they can be used in unordered containers. It uses the Component's Entity as the key value.
 struct component_hash 
 {
     entity operator()(const component& c) const { return c.get_entity(); }
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
     using DefinitionType = HashValue;

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
     void register_system_for_type(TypeId system_type, DefinitionType type);
 };

 struct entity_manager
 {
     std::mutex createMutex;

     using TypeMap = std::unordered_map<system::DefinitionType, TypeId>;    // ComponentDef type (hashed) to System TypeId map.
     TypeMap system_type_map;                                               // Map of ComponentDef type (hash) to System TypeIds.
     entity entity_counter { 0 };                                           // Autoincrementing value to generate unique Entity IDs.

     void register_system_for_type(TypeId system_type, HashValue def_type) 
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
 void system::register_system_for_type(TypeId system_type, DefinitionType type) { factory->register_system_for_type(system_type, type); }

 template <typename T>
 bool verify_typename(const char * name) 
 {
     return type_name_generator::generate<T>() == std::string(name);
 }

 ////////////////////////
 //   name/id system   //
 ////////////////////////

 struct name_system final : public system
 {
     name_system(entity_manager * f) : system(f)
     {
         register_system_for_type(this, hash_fnv1a("NameDefinition"));
     }

     ~name_system() override { }

     bool create(entity e, DefinitionType component_name_hash) override final
     {
         return false;
     }

     // Associates |entity| with a name. Removes any existing name associated with this entity.
     bool create(entity entity, HashValue type, void * data) override final
     {
         if (type != const_hash_fnv1a("NameDefinition"))  { return false; }
         return true;
         // todo - set name based on data
     }

     // Disassociates any name from |entity|.
     void destroy(entity entity) override final
     {
         auto iter = entity_to_hash_.find(entity);
         if (iter != entity_to_hash_.end()) 
         {
             hash_to_entity_.erase(iter->second);
             entity_to_hash_.erase(iter);
         }
         entity_to_name_.erase(entity);
     }

     // Finds the name associated with |entity|. Returns empty string if no name is found.
     std::string get_name(entity entity) const
     {
         const auto iter = entity_to_name_.find(entity);
         return iter != entity_to_name_.end() ? iter->second : "";
     }

     void set_name(entity entity, const std::string & name) 
     {
         if (entity == kInvalidEntity) { return; }

         const std::string existing_name = get_name(entity);
         if (existing_name == name) { return; } // No need to proceed if current name and desired name are identical

         // Ensure a different entity with the same name does not already exist. This
         // may happen if an entity with the name had not been properly deleted or
         // the same entity had been created multiple times.
         if (find_entity(name) != kInvalidEntity) 
         {
             std::cout << "entity " << name << " already exists..." << std::endl;
             return;
         }

         const auto hash = hash_fnv1a(name.c_str());

         hash_to_entity_.erase(hash_fnv1a(existing_name.c_str()));
         hash_to_entity_[hash] = entity;

         entity_to_name_[entity] = name;
         entity_to_hash_[entity] = hash;
     }

     entity find_entity(const std::string & name) const 
     {
         const auto hash = hash_fnv1a(name.c_str());
         const auto iter = hash_to_entity_.find(hash);
         return iter != hash_to_entity_.end() ? iter->second : kInvalidEntity;
     }

     std::unordered_map<entity, std::string> entity_to_name_;
     std::unordered_map<entity, HashValue> entity_to_hash_;
     std::unordered_map<HashValue, entity> hash_to_entity_;
 };
 POLYMER_SETUP_TYPEID(name_system);

 ///////////////////////////////
 //   Example Render System   //
 ///////////////////////////////

 // Systems operate on one or more Components to provide Entities with specific behaviours.
 // Entities have Components assigned by systems. 
 struct ex_render_component final : public component
 {
     ex_render_component() : component(kInvalidEntity) { }
     explicit ex_render_component(entity e) : component(e) { }
     std::string name{ "render-component" };
 };

 struct ex_render_system : public system
 {
     ex_render_system(entity_manager * f) : system(f)
     {
         register_system_for_type(this, hash_fnv1a("RenderDefinition"));
     }

     ~ex_render_system() override { }

     bool create(entity e, DefinitionType component_name_hash) override final
     {
         return false;
     }

     bool create(entity entity, HashValue type, void * data) override final
     {
         if (type != hash_fnv1a("RenderDefinition")) { return false; }

         if (components_.find(entity) != components_.end())
         {
             // update existing component
             auto data = components_[entity];
         }
         else
         {
             // create new component
             components_.emplace(entity, ex_render_component(entity));
         }
         return true;
     }
     
     // find & erase
     void destroy(entity entity) override final
     {
         auto iter = components_.find(entity);
         if (iter != components_.end())
         {
             components_.erase(iter);
         }
     }

     // returns a zero length string if not found
     std::string get_renderable_name(entity e) 
     {
         auto iter = components_.find(e);
         if (iter != components_.end()) return iter->second.name;
         return {};
     }

     // Entities added to this list have components
     std::unordered_map<entity, ex_render_component> components_;
 };
 POLYMER_SETUP_TYPEID(ex_render_system);

IMPLEMENT_MAIN(int argc, char * argv[])
{
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

    // ----

    ex_render_system renderSystem(&factory);

    // Create new entities
    const entity renderableOne = factory.create();
    const entity renderableTwo = factory.create();
    const entity renderableThree = factory.create();

    // Associate |ex_render_component| with these entities
    renderSystem.create(renderableOne, hash_fnv1a("RenderDefinition"), nullptr);
    renderSystem.create(renderableTwo, hash_fnv1a("RenderDefinition"), nullptr);

    renderSystem.get_renderable_name(renderableOne);
    renderSystem.get_renderable_name(renderableTwo);
    renderSystem.get_renderable_name(renderableThree);

    renderSystem.destroy(renderableOne);
    renderSystem.get_renderable_name(renderableOne);

    return EXIT_SUCCESS;
}