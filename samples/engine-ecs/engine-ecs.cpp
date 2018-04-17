#include "math-core.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

#include "polymer-typeid.hpp"
#include "polymer-ecs.hpp"
#include "component-pool.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

struct physics_component : public base_component
{
    physics_component() {};
    physics_component(entity e) : base_component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(physics_component);

struct render_component : public base_component
{
    render_component() {}
    render_component(entity e) : base_component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(render_component);

///////////////////
// Serialization //
///////////////////

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
    ex_system_one(entity_orchestrator * f) : base_system(f) { register_system_for_type(this, c1); }
    ~ex_system_one() override { }
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
    ex_system_two(entity_orchestrator * f) : base_system(f) { register_system_for_type(this, c2); }
    ~ex_system_two() override { }
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

//////////////////////////
//   Transform System   //
//////////////////////////

struct scene_graph_component : public base_component
{
    scene_graph_component() {};
    scene_graph_component(entity e) : base_component(e) {}
    polymer::Pose local_pose;
    polymer::float3 local_scale;
    entity parent{ kInvalidEntity };
    std::vector<entity> children;
}; POLYMER_SETUP_TYPEID(scene_graph_component);

struct world_transform_component : public base_component
{
    world_transform_component() {};
    world_transform_component(entity e) : base_component(e) {}
    polymer::Pose world_pose;
}; POLYMER_SETUP_TYPEID(world_transform_component);

class transform_system final : public base_system
{

    void recalculate_world_transform(entity child)
    {
        auto node = scene_graph_transforms.get(child);
        auto world_xform = world_transforms.get(child);

        // If the node has a parent then we can compute a new world transform.
        if (node->parent != kInvalidEntity)
        {
            auto parent_node = scene_graph_transforms.get(node->parent);
            world_xform->world_pose = node->local_pose * parent_node->local_pose;
        }
        else
        {
            // If the node has no parent, it should be considered already in world space.
            world_xform->world_pose = node->local_pose;
        }

        // For each child, calculate its new world transform
        for (auto c : node->children) recalculate_world_transform(c);
    }

    void destroy_recursive(entity child)
    {
        auto node = scene_graph_transforms.get(child);
        for (auto & n : node->children) destroy_recursive(n);

        // Erase world transform
        world_transforms.destroy(child);

        // Erase itself after all children are gone
        scene_graph_transforms.destroy(child);
    }

public:

    polymer_component_pool<scene_graph_component> scene_graph_transforms{ 64 };
    polymer_component_pool<world_transform_component> world_transforms{ 64 };

    transform_system(entity_orchestrator * f) : base_system(f) 
    { 
        register_system_for_type(this, get_typeid<scene_graph_component>()); 
        register_system_for_type(this, get_typeid<world_transform_component>());
    }

    ~transform_system() override { }

    bool create(entity e, poly_typeid hash, void * data) override final { return true; }

    bool create(entity e, const polymer::Pose local_pose, const float3 local_scale)
    {
        const auto check_node = scene_graph_transforms.get(e);
        const auto check_world = world_transforms.get(e);
        if (check_node == nullptr || check_world == nullptr)
        {
            auto node = scene_graph_transforms.emplace(scene_graph_component(e));
            auto world = world_transforms.emplace(world_transform_component(e));
            node->local_pose = local_pose;
            node->local_scale = local_scale;
            recalculate_world_transform(e);
            return true;
        }
        return false;
    }

    bool has_transform(entity e) const
    {
        return (scene_graph_transforms.get(e) != nullptr);
    }

    bool add_child(entity parent, entity child)
    {
        if (parent == kInvalidEntity) throw std::invalid_argument("parent was invalid");
        if (child == kInvalidEntity) throw std::invalid_argument("child was invalid");
        if (!has_transform(parent)) throw std::invalid_argument("parent has no transform component");
        if (!has_transform(child)) throw std::invalid_argument("child has no transform component");

        scene_graph_transforms.get(parent)->children.push_back(child);
        scene_graph_transforms.get(child)->parent = parent;
        recalculate_world_transform(parent);
        return true;
    }

    const scene_graph_component * get_local_transform(entity e)
    {
        if (e == kInvalidEntity) return nullptr;
        return scene_graph_transforms.get(e);
    }

    const world_transform_component * get_world_transform(entity e)
    {
        if (e == kInvalidEntity) return nullptr;
        return world_transforms.get(e);
    }

    bool update_local_transform(entity e, const Pose new_pose)
    {
        if (e == kInvalidEntity) return kInvalidEntity;
        if (auto * node = scene_graph_transforms.get(e))
        {
            node->local_pose = new_pose;
            recalculate_world_transform(e);
            return true;
        }
        return false;
    }

    entity get_parent(entity child) const
    {
        if (child == kInvalidEntity) return kInvalidEntity;
        if (auto * e = scene_graph_transforms.get(child)) return e->parent;
        return kInvalidEntity;
    }

    void remove_parent(entity child)
    {
        if (child == kInvalidEntity) throw std::invalid_argument("entity was invalid");
        auto child_node = scene_graph_transforms.get(child);
        if (child_node->parent != kInvalidEntity)
        {
            auto parent_node = scene_graph_transforms.get(child_node->parent);
            parent_node->children.erase(std::remove(parent_node->children.begin(), parent_node->children.end(), child), parent_node->children.end());
            child_node->parent = kInvalidEntity;
            recalculate_world_transform(child);
        }
    }

    void destroy(entity e) override final 
    { 
        if (e == kInvalidEntity) throw std::invalid_argument("entity was invalid");
        if (!has_transform(e)) throw std::invalid_argument("no component exists for this entity");
        destroy_recursive(e);
    }
};

POLYMER_SETUP_TYPEID(transform_system);

//////////////////////
//   Event System   //
//////////////////////

// note: event wrappers do not own data
struct event_wrapper
{
    poly_typeid type { 0 };          // typeid of the wrapped event.
    mutable size_t size { 0 };       // sizeof() the wrapped event.
    mutable void * data { nullptr }; // pointer to the wrapped event.

    event_wrapper() = default;
    ~event_wrapper() = default;

    template <typename E>
    explicit event_wrapper(const E & evt) 
        : type(get_typeid<E>()), size(sizeof(E)), data(const_cast<E*>(&evt)) {}

    template <typename E>
    const E * get() const
    {
        if (type != get_typeid<E>()) return nullptr;
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

class synchronous_event_manager 
{
    connection_id id{ 0 }; // autoincrementing counter
    std::shared_ptr<event_handler_map> handlers;

    // Function declaration that is used to extract a type from the event handler (const)
    template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &) const);
    template <typename Fn, typename Arg> Arg connect_helper(void (Fn::*)(const Arg &));

    connection connect_impl(poly_typeid type, const void * owner, event_handler handler) 
    {
        const connection_id new_id = ++id;
        handlers->add(type, new_id, owner, std::move(handler));
        return connection(handlers, type, new_id);
    }

public:

    synchronous_event_manager() : handlers(std::make_shared<event_handler_map>()) {}

    template <typename E>
    bool send(const E & event)
    {
        return handlers->dispatch(event_wrapper(event));
    }

    template <typename Fn>
    connection connect(const void * owner, Fn && fn)
    {
        using function_signature = typename std::remove_reference<Fn>::type;
        using event_t = decltype(connect_helper(&function_signature::operator()));

        return connect_impl(get_typeid<event_t>(), owner, [fn](const event_wrapper & event) mutable
        {
            const event_t * obj = event.get<event_t>();
            fn(*obj);
        });
    }

    template <typename Fn>
    scoped_connection connect(Fn && handler)
    {
        return connect(nullptr, std::forward<Fn>(handler));
    }

    // How many functions are currently registered?
    size_t num_handlers() const { return handlers->size(); }

    // How many functions listening for this type of event?
    size_t num_handlers_type(poly_typeid type) const { return handlers->handler_count(type); }

    //scoped_connection connect(poly_typeid type, event_handler handler) {}
};

struct example_event  { uint32_t value; }; 
POLYMER_SETUP_TYPEID(example_event);
struct example_event2 { uint32_t value; }; POLYMER_SETUP_TYPEID(example_event2);

struct handler_test
{
    void handle_event(const example_event & e) { sum += e.value; }
    uint32_t sum = 0;
};

TEST_CASE("synchronous_event_manager connection count")
{
    synchronous_event_manager manager;
    handler_test test_handler;

    REQUIRE(manager.num_handlers() == 0);
    REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 0);

    // Capture the receiving class in a lambda to invoke the handler when the event is dispatched
    auto connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });

    REQUIRE(manager.num_handlers() == 1);
    REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 1);
}

TEST_CASE("synchronous_event_manager scoped disconnection")
{
    synchronous_event_manager manager;
    handler_test test_handler;

    {
        auto scoped_connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });
    }

    REQUIRE(manager.num_handlers() == 0);
    REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 0);
}

TEST_CASE("synchronous_event_manager manual disconnection")
{
    synchronous_event_manager manager;
    handler_test test_handler;

    auto connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });

    REQUIRE(manager.num_handlers() == 1);
    REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 1);

    connection.disconnect();

    REQUIRE(manager.num_handlers() == 0);
    REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 0);

    example_event ex{ 55 };
    auto result = manager.send(ex);

    REQUIRE(result == false);
}

TEST_CASE("synchronous_event_manager connection test")
{
    synchronous_event_manager manager;

    handler_test test_handler;
    REQUIRE(test_handler.sum == 0);

    auto connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });

    example_event ex{ 5 };
    bool result = manager.send(ex);

    REQUIRE(result == true);
    REQUIRE(test_handler.sum == 5);

    for (int i = 0; i < 10; ++i)
    {
        manager.send(example_event{ 10 });
    }

    REQUIRE(test_handler.sum == 105);
}

// REQUIRE - this level will immediately quit the test case if the assert fails and will mark the test case as failed.
// CHECK - this level will mark the test case as failed if the assert fails but will continue with the test case.
// WARN - this level will only print a message if the assert fails but will not mark the test case as failed.
// REQUIRE_FALSE
// CHECK_THROWS_AS(func(), std::exception); 

////////////////////////////////
//   Transform System Tests   //
////////////////////////////////

TEST_CASE("transform system has_transform")
{
    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    entity root = orchestrator.create_entity();
    REQUIRE(system->has_transform(root) == false);

    system->create(root, Pose(), float3(1));
    REQUIRE(system->has_transform(root) == true);
}

TEST_CASE("transform system double add")
{
    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    entity root = orchestrator.create_entity();
    REQUIRE(system->create(root, Pose(), float3(1)) == true);
    REQUIRE(system->create(root, Pose(), float3(1)) == false);
}

TEST_CASE("transform system destruction")
{
    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    std::vector<entity> entities;
    for (int i = 0; i < 32; ++i)
    {
        entity e = orchestrator.create_entity();
        system->create(e, Pose(), float3(1));
        entities.push_back(e);
        REQUIRE(system->has_transform(e) == true);
    }

    for (auto & e : entities)
    {
        system->destroy(e);
        REQUIRE(system->has_transform(e) == false);
    }

    CHECK_THROWS_AS(system->destroy(0), std::exception);
}

TEST_CASE("transform system add/remove parent & children")
{
    Pose p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, POLYMER_PI / 2.0), float3(0, 5.f, 0));
    Pose p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, POLYMER_PI / 0.5), float3(3.f, 0, 0));
    Pose p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, POLYMER_PI), float3(0, 1.f, 4.f));

    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    entity root = orchestrator.create_entity();
    entity child1 = orchestrator.create_entity();
    entity child2 = orchestrator.create_entity();

    system->create(root, Pose(), float3(1));
    system->create(child1, Pose(), float3(1));
    system->create(child2, Pose(), float3(1));

    REQUIRE(system->has_transform(root) == true);
    REQUIRE(system->has_transform(child1) == true);
    REQUIRE(system->has_transform(child2) == true);

    REQUIRE(system->get_parent(root) == kInvalidEntity);
    REQUIRE(system->get_parent(child1) == kInvalidEntity);
    REQUIRE(system->get_parent(child2) == kInvalidEntity);

    CHECK_THROWS_AS(system->add_child(0, 0), std::exception); // invalid parent
    CHECK_THROWS_AS(system->add_child(root, 0), std::exception); // invalid child

    system->add_child(root, child1);
    system->add_child(root, child2);

    REQUIRE(system->get_parent(child1) == root);
    REQUIRE(system->get_parent(child2) == root);

    system->remove_parent(child1);
    REQUIRE(system->get_parent(child1) == kInvalidEntity);
}

TEST_CASE("transform system scene graph math correctness")
{
    const Pose p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, POLYMER_PI / 2.0), float3(0, 5.f, 0));
    const Pose p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, POLYMER_PI / 0.5), float3(3.f, 0, 0));
    const Pose p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, POLYMER_PI), float3(0, 1.f, 4.f));

    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    entity root = orchestrator.create_entity();
    entity child1 = orchestrator.create_entity();
    entity child2 = orchestrator.create_entity();

    system->create(root, p1, float3(1));
    system->create(child1, p2, float3(1));
    system->create(child2, p3, float3(1));

    REQUIRE(system->get_local_transform(root)->local_pose == p1);
    REQUIRE(system->get_local_transform(child1)->local_pose == p2);
    REQUIRE(system->get_local_transform(child2)->local_pose == p3);

    system->add_child(root, child1);
    system->add_child(root, child2);

    const Pose check_p1 = p1;
    const Pose check_p2 = p2 * p1;
    const Pose check_p3 = p3 * p1;

    REQUIRE(system->get_world_transform(root)->world_pose == check_p1); // root (already in worldspace)
    REQUIRE(system->get_world_transform(child1)->world_pose == check_p2);
    REQUIRE(system->get_world_transform(child2)->world_pose == check_p3);
}

TEST_CASE("transform system update local transform")
{
    // todo - test update_local_transform
}

TEST_CASE("transform system performance testing")
{
    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);
    uniform_random_gen gen;

    float timer = 0.f;
    manual_timer t;
    auto random_pose = [&]() -> Pose
    {
        t.start();
        auto p = Pose(make_rotation_quat_axis_angle({ gen.random_float(), gen.random_float(), gen.random_float() }, gen.random_float() * POLYMER_TAU),
            float3(gen.random_float() * 100, gen.random_float() * 100, gen.random_float() * 100));
        t.stop();
        timer += t.get();
        return p;
    };

    {
        scoped_timer t("create 16384 entities with 4 children each (65535 total)");
        for (int i = 0; i < 16384; ++i)
        {
            auto rootEntity = orchestrator.create_entity();
            system->create(rootEntity, random_pose(), float3(1));

            for (int c = 0; c < 4; ++c)
            {
                auto childEntity = orchestrator.create_entity();
                system->create(childEntity, random_pose(), float3(1));
                system->add_child(rootEntity, childEntity);
            }
        }

        std::cout << "Random pose generation took: " << timer << "ms" << std::endl;
    }
}

TEST_CASE("transform system performance testing 2")
{
    entity_orchestrator orchestrator;
    transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

    for (int i = 0; i < 65536; ++i)
    {
        auto rootEntity = orchestrator.create_entity();
        system->create(rootEntity, Pose(), float3(1));
    }

    {
        scoped_timer t("iterate and add");
        system->world_transforms.for_each([&](world_transform_component & t) { t.world_pose.position += float3(0.001f); });
    }
}

//////////////////////////////
//   Component Pool Tests   //
//////////////////////////////

TEST_CASE("polymer_component_pool size is zero on creation")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);
    REQUIRE(scene_graph_pool.size() == 0);
}

TEST_CASE("polymer_component_pool add elements")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);
    auto obj = scene_graph_pool.emplace(55);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->get_entity() == 55);
    REQUIRE(static_cast<int>(scene_graph_pool.size()) == 1);
}

TEST_CASE("polymer_component_pool clear elements")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);
    scene_graph_pool.emplace(99);
    scene_graph_pool.clear();
    REQUIRE(static_cast<int>(scene_graph_pool.size()) == 0);
}

TEST_CASE("polymer_component_pool contains elements")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);
    bool result = scene_graph_pool.contains(88);
    REQUIRE(result == false);

    scene_graph_pool.emplace(88);
    result = scene_graph_pool.contains(88);
    REQUIRE(result == true);
}

TEST_CASE("polymer_component_pool get elements")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);

    auto obj = scene_graph_pool.get(1);
    REQUIRE(obj == nullptr);

    scene_graph_pool.emplace(1);
    obj = scene_graph_pool.get(1);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->get_entity() == 1);
    REQUIRE(static_cast<int>(scene_graph_pool.size()) == 1);
}

TEST_CASE("polymer_component_pool check duplicate elements")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);

    scene_graph_pool.emplace(5);
    scene_graph_pool.emplace(5);
    REQUIRE(static_cast<int>(scene_graph_pool.size()) == 1);
}

TEST_CASE("polymer_component_pool add and remove")
{
    polymer_component_pool<scene_graph_component> scene_graph_pool(32);

    size_t check = 0;
    for (int i = 0; i < 128; ++i) 
    {
        const int value = 10 * i;
        auto obj = scene_graph_pool.emplace(i);
        obj->parent = value;
        check += value;
    }

    REQUIRE(static_cast<int>(scene_graph_pool.size()) ==  128);

    for (int i = 44; i < 101; ++i) 
    {
        const int value = 10 * i;
        scene_graph_pool.destroy(i);
        check -= value;
    }

    size_t sum1{ 0 }, sum2{ 0 };
    scene_graph_pool.for_each([&](scene_graph_component & t) { sum1 += t.parent; });
    for (auto & w : scene_graph_pool) { sum2 += w.parent; }

    REQUIRE(sum1 == check);
    REQUIRE(sum2 == check);
    REQUIRE(static_cast<int>(scene_graph_pool.size()) == (128 - 101 + 44));
}
