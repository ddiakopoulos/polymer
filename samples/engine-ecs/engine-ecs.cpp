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
     virtual void create(entity e, DefinitionType type) {}

     // Associates component with the Entity using the serialized data
     virtual void create(entity e, DefinitionType type, void * data) {}

     /* todo - create with default-constructed component */

     // Disassociates all Component data from the Entity.
     virtual void destroy(entity e) {}

     // Helper function to associate the System with DefType in the entity_manager.
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
 //   Example System   //
 ////////////////////////

 struct ExampleSystem : public system
 {
     std::unordered_map<entity, std::string> entity_to_name_;
     std::unordered_map<entity, HashValue> entity_to_hash_;
     std::unordered_map<HashValue, entity> hash_to_entity_;

     ExampleSystem(entity_manager * f) : system(f) 
     {
         register_system_for_type(this, hash_fnv1a("NameDefinition"));
     }

     ~ExampleSystem() override { }

     // Associates |entity| with a name. Removes any existing name associated with this entity.
     void create(entity entity, HashValue type, void * data) override final
     {
         if (type != hash_fnv1a("NameDefinition")) 
         {
             std::cout << "Invalid definition type, expecting NameDefinition." << std::endl;
             return;
         }

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
         if (entity == kInvalidEntity)
         {
             std::cout << "Invalid Entity" << std::endl;
             return;
         }

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

 };
 POLYMER_SETUP_TYPEID(ExampleSystem);

 ////////////////////////
 ////
 ////////////////////////

 // Systems operate on one or more Components to provide Entities with specific behaviours.
 // Entities have Components assigned by systems. 
 struct ExampleRenderComponent : public component
 {
     ExampleRenderComponent() : component(kInvalidEntity) { }
     explicit ExampleRenderComponent(entity e) : component(e) { }
     std::string name{ "render-component" };
 };

 struct ExampleRenderSystem : public system
 {
     ExampleRenderSystem(entity_manager * f) : system(f)
     {
         register_system_for_type(this, hash_fnv1a("RenderDefinition"));
     }

     ~ExampleRenderSystem() override { }

     void create(entity entity, HashValue type, void * data) override final
     {
         if (type != hash_fnv1a("RenderDefinition"))
         {
             std::cout << "Invalid definition type, expecting NameDefinition." << std::endl;
             return;
         }

         if (components_.find(entity) != components_.end())
         {
             std::cout << "Component Already Exists..." << std::endl;
             auto data = components_[entity];
         }
         else
         {
             std::cout << "Inserting Component... " << entity << std::endl;
             components_.emplace(entity, ExampleRenderComponent(entity));
         }
     }

     void destroy(entity entity) override final
     {
         auto iter = components_.find(entity);
         if (iter != components_.end())
         {
             std::cout << "Erasing Entity " << entity << std::endl;
             components_.erase(iter);
         }
     }

     std::string get_renderable_name(entity e) 
     {
         auto iter = components_.find(e);
         if (iter != components_.end()) return iter->second.name;
         else std::cout << "Did not find component for entity " << e << std::endl;
         return {};
     }

     // Entities added to this list have components
     std::unordered_map<entity, ExampleRenderComponent> components_;
 };
 POLYMER_SETUP_TYPEID(ExampleRenderSystem);

 /* 
  * Implement: Component Pool
  * Implement: PolymerEngineContext (Registry for Systems)
  * [Systems]
  * - Render
  * - Light
  * - Collision/Raycast
  * - Name
  * - Dispatcher (Events)
  * - Physics
  * - Transform (Scene Graph)
  * [Components / Defs]
  * 
  *
  *
  *
  *
  *
  */

IMPLEMENT_MAIN(int argc, char * argv[])
{
    /*
    entity_manager factory;
    ExampleSystem system(&factory);

    const Entity exampleEntity = factory.create();

    system.set_name(exampleEntity, "static_mesh");
    std::cout << "Lookup By Name:   " << system.get_name(exampleEntity) << std::endl;
    std::cout << "Lookup By Entity: " << system.find_entity("static_mesh") << std::endl;

    std::cout << "verify: bool - " << verify_typename<bool>("bool") << std::endl;
    std::cout << "verify: uint64_t - " << verify_typename<uint64_t>("uint64_t") << std::endl;
    std::cout << "verify: float2 - " << verify_typename<float2>("float2") << std::endl;
    std::cout << "verify: Bounds2D - " << verify_typename<Bounds2D>("Bounds2D") << std::endl;
    */

    entity_manager factory;

    ExampleRenderSystem renderSystem(&factory);

    // Create new entities
    const entity renderableOne = factory.create();
    const entity renderableTwo = factory.create();
    const entity renderableThree = factory.create();

    // Associate |ExampleRenderComponent| with these entities
    renderSystem.create(renderableOne, hash_fnv1a("RenderDefinition"), nullptr);
    renderSystem.create(renderableTwo, hash_fnv1a("RenderDefinition"), nullptr);

    renderSystem.get_renderable_name(renderableOne);
    renderSystem.get_renderable_name(renderableTwo);
    renderSystem.get_renderable_name(renderableThree);

    renderSystem.destroy(renderableOne);
    renderSystem.get_renderable_name(renderableOne);

    return EXIT_SUCCESS;
}