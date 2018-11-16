#include "math-core.hpp"

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/component-pool.hpp"
#include "ecs/core-events.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "ui-actions.hpp"

/// Quick reference for doctest macros
/// REQUIRE, REQUIRE_FALSE, CHECK, WARN, CHECK_THROWS_AS(func(), std::exception)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace polymer
{
    /////////////////////
    //   Event Tests   //
    /////////////////////

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
        transform p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, (float) POLYMER_PI / 2.0f), float3(0, 5.f, 0));
        transform p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, (float) POLYMER_PI / 0.5f), float3(3.f, 0, 0));
        transform p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, (float) POLYMER_PI), float3(0, 1.f, 4.f));

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

        system->remove_parent_from_child(child1);
        REQUIRE(system->get_parent(child1) == kInvalidEntity);
    }

    TEST_CASE("transform system scene graph math correctness")
    {
        const transform p1(make_rotation_quat_axis_angle({ 0, 1, 0 }, (float) POLYMER_PI / 2.0f), float3(0, 5.f, 0));
        const transform p2(make_rotation_quat_axis_angle({ 1, 1, 0 }, (float) POLYMER_PI / 0.5f), float3(3.f, 0, 0));
        const transform p3(make_rotation_quat_axis_angle({ 0, 1, -1 }, (float) POLYMER_PI), float3(0, 1.f, 4.f));

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
        const transform check_p2 = p1 * p2;
        const transform check_p3 = p1 * p3;

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

    TEST_CASE("transform system set local transform")
    {
        // todo - test set_local_transform
    }

    TEST_CASE("transform system performance testing")
    {
        entity_orchestrator orchestrator;
        transform_system * system = orchestrator.create_system<transform_system>(&orchestrator);
        uniform_random_gen gen;

        double timer = 0.f;
        manual_timer t;
        auto random_pose = [&]() -> transform
        {
            t.start();
            auto p = transform(make_rotation_quat_axis_angle({ gen.random_float(), gen.random_float(), gen.random_float() }, gen.random_float() * (float) POLYMER_TAU),
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

    /////////////////////////////////
    //   Identifier System Tests   //
    /////////////////////////////////

    TEST_CASE("identifier_system unified tests")
    {
        entity_orchestrator orchestrator;
        identifier_system * system = orchestrator.create_system<identifier_system>(&orchestrator);

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

    ///////////////////////////////////////
    //   Property + Undo Manager Tests   //
    ///////////////////////////////////////

    TEST_CASE("undo_manager max stack size")
    {
        undo_manager manager;
        polymer::property<uint32_t> val(0);

        // Default stack size is 64. Load up slots all the way up to 64+32.  
        for (uint32_t i = 0; i <= manager.get_max_stack_size() + 32; ++i)
        {
            auto edit = make_action<action_edit_property>(val, i);
            manager.execute(std::move(edit));
        }

        // Execute `get_max_stack_size()`  undos. Our last value should be 32. 
        for (uint32_t i = 0; i < manager.get_max_stack_size(); ++i)
        {
            manager.undo();
        }

        REQUIRE(val.value() == 32u);
    }


    TEST_CASE("undo_manager can undo/redo")
    {
        undo_manager manager;
        polymer::property<uint32_t> val(0);

        for (uint32_t i = 0; i < 32; ++i)
        {
            auto edit = make_action<action_edit_property>(val, i);
            manager.execute(std::move(edit));
        }

        REQUIRE(manager.can_undo() == true);

        REQUIRE(manager.can_redo() == false);
        manager.undo();

        REQUIRE(manager.can_redo() == true);
        manager.redo();

        REQUIRE(manager.can_redo() == false);

        manager.clear();

        REQUIRE(manager.can_undo() == false);
        REQUIRE(manager.can_redo() == false);
    }

    TEST_CASE("action_edit_property ... with undo")
    {
        undo_manager manager;

        polymer::property<float> v(0.5f);
        REQUIRE(v.value() == 0.5f);

        auto edit = make_action<action_edit_property>(v, 2.f);
        manager.execute(std::move(edit));
        REQUIRE(v.value() == 2.f);

        manager.undo();
        REQUIRE(v.value() == 0.5f);

        manager.redo();
        REQUIRE(v.value() == 2.0f);
    }

    TEST_CASE("action_edit_property ... with multi-undo")
    {
        undo_manager manager;

        polymer::property<uint32_t> v(10u);
        REQUIRE(v.value() == 10);

        auto edit1 = make_action<action_edit_property>(v, 20u);
        manager.execute(std::move(edit1));
        REQUIRE(v.value() == 20u);

        auto edit2 = make_action<action_edit_property>(v, 30u);
        manager.execute(std::move(edit2));
        REQUIRE(v.value() == 30u);

        auto edit3 = make_action<action_edit_property>(v, 40u);
        manager.execute(std::move(edit3));
        REQUIRE(v.value() == 40u);

        manager.undo();
        REQUIRE(v.value() == 30u);

        manager.undo();
        REQUIRE(v.value() == 20u);

        manager.undo();
        REQUIRE(v.value() == 10u);

        manager.redo();
        manager.redo();
        manager.redo();

        REQUIRE(v.value() == 40u);
    }

    TEST_CASE("polymer::property operators")
    {
        polymer::property<float> property_a(0.5f);
        polymer::property<float> property_b(0.5f);
        polymer::property<float> property_c(1.0f);

        REQUIRE(property_a == property_b);
        REQUIRE(property_a != property_c);
        REQUIRE_FALSE(property_a == property_c);

        std::cout << "property a: " << property_a << std::endl;
        std::cout << "property c: " << property_c << std::endl;
    }

    TEST_CASE("poly_property kernel_set")
    {
        struct sky
        {
            void recompute_parameters()
            {
                std::cout << "Recomputing sky parameters..." << std::endl;
            }
        };

        sky scene_sky;
        polymer::property<uint32_t> sky_turbidity;

        sky_turbidity.kernel_set([&scene_sky](uint32_t v) -> uint32_t
        {
            scene_sky.recompute_parameters();
            return v + 10;
        });

        sky_turbidity.set(5);

        REQUIRE(sky_turbidity == 15);
    }

} // end namespace polymer

