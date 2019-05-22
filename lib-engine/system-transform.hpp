#pragma once

#ifndef polymer_system_transform_hpp
#define polymer_system_transform_hpp

#include "ecs/typeid.hpp"
#include "ecs/core-ecs.hpp"
#include "ecs/component-pool.hpp"
#include "serialization.hpp"
#include "environment.hpp"

namespace polymer
{
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
            // Note that during deserialization we might not have created the parent yet
            // so we are allowed to no-op given a null parent node
            if (node->parent != kInvalidEntity)
            {
                auto parent_node = scene_graph_transforms.get(node->parent);

                if (parent_node)
                {
                    world_xform->world_pose = parent_node->local_pose * node->local_pose;
                }
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
            // Remove parent assocations first
            remove_parent_from_child(child);

            auto node = scene_graph_transforms.get(child);
            if (node) for (auto & n : node->children) destroy_recursive(n);

            // Erase world transform
            world_transforms.destroy(child);

            // Erase itself after all children are gone
            scene_graph_transforms.destroy(child);
        }

        // Resolve orphans. For instance, if we change the parent of an entity using the UI, it never gets added to
        // the list of children of the parent. 
        void fix_parent_child_orphans()
        {
            scene_graph_transforms.for_each([&](local_transform_component & t)
            {
                const auto entity = t.get_entity();
                auto parent = get_parent(entity);
                if (parent != kInvalidEntity)
                {
                    // If we have a parent, check if its list of children has this entity
                    if (!has_child(parent, entity))
                    {
                        std::cout << "Found orphan relationship: " << parent << ", " << entity << std::endl;
                        add_child(parent, entity);
                    }
                }
            });
        }

        template<class F> friend void visit_components(entity e, transform_system * system, F f);

    public:

        polymer_component_pool<local_transform_component> scene_graph_transforms{ 64 };
        polymer_component_pool<world_transform_component> world_transforms{ 64 };

        transform_system(entity_orchestrator * f) : base_system(f)
        {
            register_system_for_type(this, get_typeid<local_transform_component>());
        }

        ~transform_system() override { }

        bool create(entity e, poly_typeid hash, void * data) override final 
        { 
            if (hash != get_typeid<local_transform_component>()) { std::cout << "FUCL!\n"; return false; }
            auto new_component = static_cast<local_transform_component *>(data);
            return create(e, new_component->local_pose, new_component->local_scale, new_component->parent, new_component->children);
        }

        bool create(entity e, 
            const transform local_pose, 
            const float3 local_scale = float3(1, 1, 1), 
            const entity parent = kInvalidEntity, 
            const std::vector<entity> & children = {})
        {
            const auto check_node = scene_graph_transforms.get(e);
            const auto check_world = world_transforms.get(e);
            if (check_node == nullptr || check_world == nullptr) // and vs. or?
            {
                auto node = scene_graph_transforms.emplace(local_transform_component(e));
                auto world = world_transforms.emplace(world_transform_component(e));
                node->local_pose = local_pose;
                node->local_scale = local_scale;
                node->children = children;
                node->parent = parent;
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
            if (parent == child) throw std::invalid_argument("parent and child cannot be the same");
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

        const local_transform_component * get_local_transform(entity e)
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

        bool has_child(entity parent, entity child) const
        {
            if (parent == kInvalidEntity) return false;
            if (child == kInvalidEntity) return false;

            bool found_child = false;
            if (auto * e = scene_graph_transforms.get(parent))
            {
                for (auto one_child : e->children)
                {
                    if (one_child == child)
                    {
                        found_child = true;
                    }
                }
            }
            return found_child;
        }

        void remove_parent_from_child(entity child)
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

        void refresh()
        {
           scene_graph_transforms.for_each([&](local_transform_component & t) 
           { 
               const auto entity = t.get_entity();
               if (entity != kInvalidEntity) recalculate_world_transform(entity);
           });
        }
    };

    template<class F> void visit_components(entity e, transform_system * system, F f)
    {
        local_transform_component * component = system->scene_graph_transforms.get(e);
        if (component != nullptr) f("transform component", *component);

        // while inspecting, we need to continuously recalculate based on potentially changed data
        system->fix_parent_child_orphans();
        system->recalculate_world_transform(e);

    }

    POLYMER_SETUP_TYPEID(transform_system);

} // end namespace polymer

#endif // end polymer_system_transform_hpp