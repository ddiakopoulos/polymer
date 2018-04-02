#include "index.hpp"
#include <unordered_map>

using namespace polymer;

///////////////////////////////////////
//   Compile-Time Constant Hashing   //
///////////////////////////////////////

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

using HashValue = uint64_t;
constexpr HashValue hash_offset_basis = 0x84222325;
constexpr HashValue hash_prime_multiplier = 0x000001b3;

namespace detail 
{
    // Helper function for performing the recursion for the compile time hash.
    template <std::size_t N>
    inline constexpr HashValue ConstHash(const char(&str)[N], int start, HashValue hash) 
    {
        // Perform the static_cast to uint64_t otherwise MSVC complains, about integral constant overflow (warning C4307).
        return (start == N || start == N - 1) ? hash : ConstHash(str, start + 1, static_cast<HashValue>((hash ^ static_cast<unsigned char>(str[start])) * static_cast<uint64_t>(hash_prime_multiplier)));
    }
}  // namespace detail

// Compile-time hash function.
template <std::size_t N>
inline constexpr HashValue ConstHash(const char(&str)[N])
{
    return N <= 1 ? 0 : detail::ConstHash(str, 0, hash_offset_basis);
}

HashValue Hash(HashValue basis, const char * str, size_t len)
{
    if (str == nullptr || *str == 0 || len == 0) return 0;

    size_t count = 0;
    HashValue value = basis;
    while (*str && count < len)
    {
        value = (value ^ static_cast<unsigned char>(*str++)) * hash_prime_multiplier;
        ++count;
    }
    return value;
}

HashValue Hash(const char * str, size_t len)
{
    return Hash(hash_offset_basis, str, len);
}

HashValue Hash(const char * str)
{
    const size_t npos = -1;
    return Hash(str, npos);
}

// Functor for hashable types in STL containers.
struct Hasher 
{
    template <class T>
    std::size_t operator()(const T & value) const { return Hash(value); }
};

//////////////////////////////////////
//   Custom TypeId Implementation   //
//////////////////////////////////////

using TypeId = uint64_t;

template <typename T>
const char * GetTypeName() 
{
    assert(false);
    // If you get a compiler error that complains about TYPEID_NOT_SETUP, then you have not added 
    // POLYMER_SETUP_TYPEID(T) such that the compilation unit being compiled uses it.
    return T::TYPEID_NOT_SETUP;
}

template <typename T>
TypeId GetTypeId() { return T::TYPEID_NOT_SETUP; }

template <typename T>
struct TypeIdTraits { static constexpr bool kHasTypeId = false; };

#define POLYMER_SETUP_TYPEID(Type)             \
template <>                                    \
struct TypeIdTraits<Type> {                    \
    static constexpr bool kHasTypeId = true;   \
};                                             \
template <>                                    \
    inline const char* GetTypeName<Type>() {   \
    return #Type;                              \
}                                              \
template <>                                    \
    inline TypeId GetTypeId<Type>() {          \
    return ConstHash(#Type);                   \
}                                          

 /////////////////////////
 //   TypeId Registry   //
 /////////////////////////

 // Used for SFINAE on other types we haven't setup yet (stl containers)
 template <typename T>
 struct has_typename { static const bool value = true; };

 class type_name_generator 
 {
     template <typename T>
     static auto generate_impl() -> typename std::enable_if<has_typename<T>::value, std::string>::type
     {
         return GetTypeName<T>();
     }
 public:
     /// Returns the name of the specified type |T|.
     template <typename T>
     static std::string generate()  { return generate_impl<T>(); }
 };

 //////////////////
 //   Entities   //
 //////////////////

 // An Entity is an uniquely identifiable object in the Polymer runtime.
 using Entity = uint64_t;
 constexpr Entity kInvalidEntity = 0;

 // Basic Types
 POLYMER_SETUP_TYPEID(bool);
 POLYMER_SETUP_TYPEID(float);
 POLYMER_SETUP_TYPEID(double);
 POLYMER_SETUP_TYPEID(int8_t);
 POLYMER_SETUP_TYPEID(uint8_t);
 POLYMER_SETUP_TYPEID(int16_t);
 POLYMER_SETUP_TYPEID(uint16_t);
 POLYMER_SETUP_TYPEID(int32_t);
 POLYMER_SETUP_TYPEID(uint32_t);
 POLYMER_SETUP_TYPEID(int64_t);
 POLYMER_SETUP_TYPEID(uint64_t);

 // Linalg & Polymer Types
 POLYMER_SETUP_TYPEID(float2);
 POLYMER_SETUP_TYPEID(float3);
 POLYMER_SETUP_TYPEID(float4);
 POLYMER_SETUP_TYPEID(int2);
 POLYMER_SETUP_TYPEID(int3);
 POLYMER_SETUP_TYPEID(int4);
 POLYMER_SETUP_TYPEID(uint2);
 POLYMER_SETUP_TYPEID(uint3);
 POLYMER_SETUP_TYPEID(uint4);
 POLYMER_SETUP_TYPEID(float2x2);
 POLYMER_SETUP_TYPEID(float3x3);
 POLYMER_SETUP_TYPEID(float4x4);
 POLYMER_SETUP_TYPEID(Frustum);
 POLYMER_SETUP_TYPEID(Pose);
 POLYMER_SETUP_TYPEID(Bounds2D);
 POLYMER_SETUP_TYPEID(Bounds3D);

 // Provide a consistent way to retrieve an Entity to which a component belongs. 
 class Component 
 {
     Entity ent{ kInvalidEntity };
 public:
     explicit Component(Entity e) : ent(e) { }
     Entity get_entity() const { return ent; }
 };

 // Hash functor for Components so they can be used in unordered containers. It uses the Component's Entity as the key value.
 struct ComponentHash 
 {
     Entity operator()(const Component& c) const { return c.get_entity(); }
 };

 // Systems are responsible for storing the component data instances associated with Entities.
 // They also perform all the logic for manipulating and processing their Components.
 // This base class provides an API for an EntityFactory to associate Components with Entities in a data-driven manner.
 struct EntityFactory;
 struct System : public non_copyable
 {
     EntityFactory * factory;

     explicit System(EntityFactory * f) : factory(f) { }
     virtual ~System() { }

     // The Hash of the actual type used for safely casting the DefinitionType to a concrete type for extracting data.
     using DefinitionType = HashValue;

     // Associates Component(s) with the Entity using the serialized data
     virtual void create(Entity e, DefinitionType type, void * data) { }

     /* todo - create with default-constructed component */

     // Disassociates all Component data from the Entity.
     virtual void destroy(Entity e) { }

     // Helper function to associate the System with DefType in the EntityFactory.
     // Example usage: register_system_for_type(this, Hash("MyComponentDef"));
     template <typename S>
     void register_system_for_type(S * system, DefinitionType type) { register_system_for_type(GetTypeId<S>(), type); }
     void register_system_for_type(TypeId system_type, DefinitionType type);
 };

 struct EntityFactory
 {
     std::mutex createMutex;

     using TypeMap = std::unordered_map<System::DefinitionType, TypeId>;    // ComponentDef type (hashed) to System TypeId map.
     TypeMap system_type_map;                                               // Map of ComponentDef type (hash) to System TypeIds.
     Entity entity_counter { 0 };                                           // Autoincrementing value to generate unique Entity IDs.

     void register_system_for_type(TypeId system_type, HashValue def_type) 
     {
         system_type_map[def_type] = system_type;
     }

     Entity create()
     {
         std::lock_guard<std::mutex> guard(createMutex);
         const Entity e = ++entity_counter;
         return e;
     }
 };

 // Associates the System with the DefType in the EntityFactory.
 void System::register_system_for_type(TypeId system_type, DefinitionType type) { factory->register_system_for_type(system_type, type); }

 template <typename T>
 bool verify_typename(const char * name) 
 {
     return type_name_generator::generate<T>() == std::string(name);
 }

 ////////////////////////
 //   Example System   //
 ////////////////////////

 struct ExampleSystem : public System
 {
     std::unordered_map<Entity, std::string> entity_to_name_;
     std::unordered_map<Entity, HashValue> entity_to_hash_;
     std::unordered_map<HashValue, Entity> hash_to_entity_;

     ExampleSystem(EntityFactory * f) : System(f) 
     {
         register_system_for_type(this, Hash("NameDefinition"));
     }

     ~ExampleSystem() override { }

     // Associates |entity| with a name. Removes any existing name associated with this entity.
     void create(Entity entity, HashValue type, void * data) override final
     {
         if (type != Hash("NameDefinition")) 
         {
             std::cout << "Invalid definition type, expecting NameDefinition." << std::endl;
             return;
         }

         // todo - set name based on data
     }

     // Disassociates any name from |entity|.
     void destroy(Entity entity) override final
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
     std::string get_name(Entity entity) const
     {
         const auto iter = entity_to_name_.find(entity);
         return iter != entity_to_name_.end() ? iter->second : "";
     }

     void set_name(Entity entity, const std::string & name) 
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

         const auto hash = Hash(name.c_str());

         hash_to_entity_.erase(Hash(existing_name.c_str()));
         hash_to_entity_[hash] = entity;

         entity_to_name_[entity] = name;
         entity_to_hash_[entity] = hash;
     }

     Entity find_entity(const std::string & name) const 
     {
         const auto hash = Hash(name.c_str());
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
 struct ExampleRenderComponent : public Component
 {
     ExampleRenderComponent() : Component(kInvalidEntity) { }
     explicit ExampleRenderComponent(Entity e) : Component(e) { }
     std::string name{ "render-component" };
 };

 struct ExampleRenderSystem : public System
 {
     ExampleRenderSystem(EntityFactory * f) : System(f)
     {
         register_system_for_type(this, Hash("RenderDefinition"));
     }

     ~ExampleRenderSystem() override { }

     void create(Entity entity, HashValue type, void * data) override final
     {
         if (type != Hash("RenderDefinition"))
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

     void destroy(Entity entity) override final
     {
         auto iter = components_.find(entity);
         if (iter != components_.end())
         {
             std::cout << "Erasing Entity " << entity << std::endl;
             components_.erase(iter);
         }
     }

     std::string get_renderable_name(Entity e) 
     {
         auto iter = components_.find(e);
         if (iter != components_.end()) return iter->second.name;
         else std::cout << "Did not find component for entity " << e << std::endl;
         return {};
     }

     // Entities added to this list have components
     std::unordered_map<Entity, ExampleRenderComponent> components_;
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
    EntityFactory factory;
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

    EntityFactory factory;

    ExampleRenderSystem renderSystem(&factory);

    // Create new entities
    const Entity renderableOne = factory.create();
    const Entity renderableTwo = factory.create();
    const Entity renderableThree = factory.create();

    // Associate |ExampleRenderComponent| with these entities
    renderSystem.create(renderableOne, Hash("RenderDefinition"), nullptr);
    renderSystem.create(renderableTwo, Hash("RenderDefinition"), nullptr);

    renderSystem.get_renderable_name(renderableOne);
    renderSystem.get_renderable_name(renderableTwo);
    renderSystem.get_renderable_name(renderableThree);

    renderSystem.destroy(renderableOne);
    renderSystem.get_renderable_name(renderableOne);

    return EXIT_SUCCESS;
}