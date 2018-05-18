#include "math-core.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/system-transform.hpp"
#include "ecs/system-name.hpp"
#include "ecs/component-pool.hpp"
#include "ecs/core-events.hpp"

/// Quick reference for doctest macros
/// REQUIRE, REQUIRE_FALSE, CHECK, WARN, CHECK_THROWS_AS(func(), std::exception)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace cereal
{
    void prologue(cereal::JSONOutputArchive & archive, scene_graph_component const & o) {}
    void epilogue(cereal::JSONOutputArchive & archive, scene_graph_component const & o) {}
    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }
    template<class Archive> void serialize(Archive & archive, transform & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }
}

namespace polymer
{

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

    /*
    template<class F> void visit_fields(scene_graph_component & o, F f)
    {
        f("local_pose", o.local_pose);
        f("local_scale", o.local_scale);
        f("parent", o.parent);
        f("children", o.children);
    }
    */

    template<class Archive> void serialize(Archive & archive, entity & e)
    {
        archive(cereal::make_nvp("child", (uint64_t)e));
    }

    template<class Archive> void serialize(Archive & archive, scene_graph_component & m)
    {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata)
        {
            archive(cereal::make_nvp(name, field));
        });
    }

    struct example_event { uint32_t value; };
    POLYMER_SETUP_TYPEID(example_event);

    struct example_event2 { uint32_t value; };
    POLYMER_SETUP_TYPEID(example_event2);

    struct queued_event
    {
        queued_event() {}
        explicit queued_event(uint32_t value) : value(value) {}
        queued_event(uint32_t value, const std::string & text) : value(value), text(text) {}
        const uint32_t value = 0;
        const std::string text;
    };
    POLYMER_SETUP_TYPEID(queued_event);

    struct example_queued_event_handler
    {
        example_queued_event_handler() { static_value = 0; static_accumulator = 0; }
        void handle(const queued_event & e) { accumulator += e.value; value = e.value; text = e.text; }
        static void static_handle(const queued_event & e) { static_value = e.value; static_accumulator += e.value; }
        std::string text;
        uint32_t value = 0;
        uint32_t accumulator = 0;
        static uint32_t static_value;
        static uint32_t static_accumulator;
    };

    uint32_t example_queued_event_handler::static_value = 0;
    uint32_t example_queued_event_handler::static_accumulator = 0;

    struct handler_test
    {
        void handle_event(const example_event & e) { sum += e.value; }
        uint32_t sum = 0;
    };

    TEST_CASE("event_manager_sync connection count")
    {
        event_manager_sync manager;
        handler_test test_handler;

        REQUIRE(manager.num_handlers() == 0);
        REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 0);

        // Capture the receiving class in a lambda to invoke the handler when the event is dispatched
        auto connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });

        REQUIRE(manager.num_handlers() == 1);
        REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 1);
    }

    TEST_CASE("event_manager_sync scoped disconnection")
    {
        event_manager_sync manager;
        handler_test test_handler;

        {
            auto scoped_connection = manager.connect([&](const example_event & event) { test_handler.handle_event(event); });
        }

        REQUIRE(manager.num_handlers() == 0);
        REQUIRE(manager.num_handlers_type(get_typeid<example_event>()) == 0);
    }

    TEST_CASE("event_manager_sync manual disconnection")
    {
        event_manager_sync manager;
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

    TEST_CASE("event_manager_sync connection by type and handler")
    {
        // connect(poly_typeid type, event_handler handler)
    }

    TEST_CASE("event_manager_sync connect all ")
    {
        // connect_all(event_handler handler)
    }

    TEST_CASE("event_manager_sync disconnect type by owner pointer")
    {
        //disconnect<T>(const void * owner)
    }

    TEST_CASE("event_manager_sync disconnect by type and owner")
    {
        // disconnect(poly_typeid type, const void * owner)
    }

    TEST_CASE("event_manager_sync disconnect all by owner")
    {
        //disconnect_all(const void * owner)
    }

    TEST_CASE("event_manager_sync connection test")
    {
        event_manager_sync manager;

        handler_test test_handler;
        REQUIRE(test_handler.sum == 0);

        auto connection = manager.connect([&](const example_event & event)
        {
            test_handler.handle_event(event);
        });

        example_event ex{ 5 };
        bool result = manager.send(ex);

        REQUIRE(result);
        REQUIRE(test_handler.sum == 5);

        for (int i = 0; i < 10; ++i)
        {
            manager.send(example_event{ 10 });
        }

        REQUIRE(test_handler.sum == 105);
    }

    TEST_CASE("event_manager_async")
    {
        event_manager_async mgr;
        example_queued_event_handler handlerClass;

        auto c1 = mgr.connect([&](const queued_event & event) {
            example_queued_event_handler::static_handle(event);
        });

        auto c2 = mgr.connect([&](const queued_event & event) { handlerClass.handle(event); });

        std::vector<std::thread> producerThreads;
        static const int num_producers = 64;

        for (int i = 0; i < num_producers; ++i)
        {
            producerThreads.emplace_back([&mgr]()
            {
                for (int j = 0; j < 64; ++j)
                {
                    mgr.send(queued_event{ (uint32_t)j + 1 });
                }
            });
        }

        for (auto & t : producerThreads) t.join();

        REQUIRE(0 == handlerClass.accumulator);
        REQUIRE(0 == handlerClass.static_accumulator);

        // Process queue on the main thread by dispatching all events here
        mgr.process();

        REQUIRE(2080 * num_producers == handlerClass.accumulator);
        REQUIRE(2080 * num_producers == handlerClass.static_accumulator);
    }

    ////////////////////////////////
    //   Transform System Tests   //
    ////////////////////////////////

    TEST_CASE("transform system has_transform")
    {
        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

        entity root = orchestrator.create_entity();
        REQUIRE_FALSE(system->has_transform(root));

        system->create(root, transform(), float3(1));
        REQUIRE(system->has_transform(root));
    }

    TEST_CASE("transform system double add")
    {
        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

        entity root = orchestrator.create_entity();
        REQUIRE(system->create(root, transform(), float3(1)));
        REQUIRE_FALSE(system->create(root, transform(), float3(1)));
    }

    TEST_CASE("transform system destruction")
    {
        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

        std::vector<entity> entities;
        for (int i = 0; i < 32; ++i)
        {
            entity e = orchestrator.create_entity();
            system->create(e, transform(), float3(1));
            entities.push_back(e);
            REQUIRE(system->has_transform(e));
        }

        for (auto & e : entities)
        {
            system->destroy(e);
            REQUIRE_FALSE(system->has_transform(e));
        }

        CHECK_THROWS_AS(system->destroy(0), std::exception);
    }

    TEST_CASE("transform system add/remove parent & children")
    {
        transform p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, POLYMER_PI / 2.0), float3(0, 5.f, 0));
        transform p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, POLYMER_PI / 0.5), float3(3.f, 0, 0));
        transform p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, POLYMER_PI), float3(0, 1.f, 4.f));

        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

        entity root = orchestrator.create_entity();
        entity child1 = orchestrator.create_entity();
        entity child2 = orchestrator.create_entity();

        system->create(root, transform(), float3(1));
        system->create(child1, transform(), float3(1));
        system->create(child2, transform(), float3(1));

        REQUIRE(system->has_transform(root));
        REQUIRE(system->has_transform(child1));
        REQUIRE(system->has_transform(child2));

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
        const transform p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, POLYMER_PI / 2.0), float3(0, 5.f, 0));
        const transform p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, POLYMER_PI / 0.5), float3(3.f, 0, 0));
        const transform p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, POLYMER_PI), float3(0, 1.f, 4.f));

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

        const transform check_p1 = p1;
        const transform check_p2 = p2 * p1;
        const transform check_p3 = p3 * p1;

        REQUIRE(system->get_world_transform(root)->world_pose == check_p1); // root (already in worldspace)
        REQUIRE(system->get_world_transform(child1)->world_pose == check_p2);
        REQUIRE(system->get_world_transform(child2)->world_pose == check_p3);
    }

    TEST_CASE("transform system insert child via index")
    {
        // todo - test insert_child
    }

    TEST_CASE("transform system move child via index")
    {
        // todo - test move_child
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
        auto random_pose = [&]() -> transform
        {
            t.start();
            auto p = transform(make_rotation_quat_axis_angle({ gen.random_float(), gen.random_float(), gen.random_float() }, gen.random_float() * POLYMER_TAU),
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
            system->create(rootEntity, transform(), float3(1));
        }

        {
            scoped_timer t("iterate and add");
            system->world_transforms.for_each([&](world_transform_component & t) { t.world_pose.position += float3(0.001f); });
        }
    }


    TEST_CASE("transform system serialization test")
    {
        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);

        /*
        render_component serialize(orchestrator.create_entity());
        serialize.value1 = 1.f;
        serialize.value2 = 2.f;
        serialize.value3 = 3.f;
        auto str = serialize_to_json(serialize);

        render_component deserialize = (orchestrator.create_entity());
        deserialize_from_json(str, deserialize);
        */

        entity root = orchestrator.create_entity();
        entity child1 = orchestrator.create_entity();
        entity child2 = orchestrator.create_entity();

        system->create(root, transform(), float3(1));
        system->create(child1, transform(), float3(1));
        system->create(child2, transform(), float3(1));

        system->add_child(root, child1);
        system->add_child(root, child2);

        // Ideally should serialize an entity
        // Find all systems using entity
        // Serialize those components as an entity block

        auto str = serialize_to_json(*system->get_local_transform(root));
        std::cout << str << std::endl;

        system->scene_graph_transforms.for_each([&](scene_graph_component & t)
        {
            // std::cout << "e? " << t.get_entity() << std::endl;
        });

        std::cout << "Serialize Test" << std::endl;

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
        REQUIRE_FALSE(result);

        scene_graph_pool.emplace(88);
        result = scene_graph_pool.contains(88);
        REQUIRE(result);
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

        REQUIRE(static_cast<int>(scene_graph_pool.size()) == 128);

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

    ///////////////////////////
    //   Name System Tests   //
    ///////////////////////////

    TEST_CASE("name_system unified tests")
    {
        entity_orchestrator orchestrator;
        name_system * system = orchestrator.create_system<name_system>(&orchestrator);

        const entity e1 = orchestrator.create_entity();
        const entity e2 = orchestrator.create_entity();
        const entity e3 = orchestrator.create_entity();

        REQUIRE_FALSE(system->create(0, "oops"));

        REQUIRE(system->create(e1, "first-entity"));
        REQUIRE(system->create(e2, "second-entity"));

        // Throws on duplicate name
        CHECK_THROWS_AS(system->create(e1, "first-entity"), std::runtime_error);

        REQUIRE(system->get_name(e1) == "first-entity");
        REQUIRE(system->get_name(e2) == "second-entity");

        REQUIRE(system->find_entity("first-entity") == e1);
        REQUIRE(system->find_entity("second-entity") == e2);
        REQUIRE(system->find_entity("sjdhfk") == kInvalidEntity);
        REQUIRE(system->find_entity("") == kInvalidEntity);

        // Destroy e1
        system->destroy(e1);
        REQUIRE(system->find_entity("first-entity") == kInvalidEntity);
        REQUIRE(system->get_name(e1).empty());

        // Re-create e1
        REQUIRE(system->create(e1, "first-entity"));
        REQUIRE(system->find_entity("first-entity") == e1);

        // Modify name of e2
        REQUIRE(system->set_name(e2, "second-entity-modified"));
        REQUIRE(system->find_entity("second-entity-modified") == e2);
    }

} // end namespace polymer

