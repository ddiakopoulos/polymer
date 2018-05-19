#pragma once

#ifndef polymer_system_transform_hpp
#define polymer_system_transform_hpp

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/component-pool.hpp"
#include "serialization.inl"

namespace polymer
{

    //////////////////////////////
    //   Transform Components   //
    //////////////////////////////

    struct scene_graph_component : public base_component
    {
        scene_graph_component() {};
        scene_graph_component(entity e) : base_component(e) {}
        polymer::transform local_pose;
        polymer::float3 local_scale;
        entity parent{ kInvalidEntity };
        std::vector<entity> children;
    };
    POLYMER_SETUP_TYPEID(scene_graph_component);

    template<class F> void visit_fields(scene_graph_component & o, F f)
    {
        f("local_pose", o.local_pose);
        f("local_scale", o.local_scale);
        f("parent", o.parent);
        f("children", o.children, editor_hidden{});
    }

    struct world_transform_component : public base_component
    {
        world_transform_component() {};
        world_transform_component(entity e) : base_component(e) {}
        polymer::transform world_pose;
    };
    POLYMER_SETUP_TYPEID(world_transform_component);

    //////////////////////////
    //   Transform System   //
    //////////////////////////

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

        template<class F> friend void visit_component_fields(entity e, transform_system * system, F f);

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

        bool create(entity e, const transform local_pose, const float3 local_scale)
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

        void insert_child(entity parent, entity child, uint32_t idx = -1) { /* todo */ }

        void move_child(entity child, uint32_t idx) { /* todo */ }

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

        bool set_local_transform(entity e, const transform new_transform)
        {
            if (e == kInvalidEntity) return kInvalidEntity;
            if (auto * node = scene_graph_transforms.get(e))
            {
                node->local_pose = new_transform;
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

    template<class F> void visit_component_fields(entity e, transform_system * system, F f)
    {
        scene_graph_component * component = system->scene_graph_transforms.get(e);
        if (component != nullptr)
        {
            visit_fields(*component, [&](const char * name, auto & field, auto... metadata)
            {
                f(name, field, metadata...); // f at this point is build_imgui
            });
            system->recalculate_world_transform(e);
        }
    }

    POLYMER_SETUP_TYPEID(transform_system);

} // end namespace polymer

#endif // end polymer_system_transform_hpp