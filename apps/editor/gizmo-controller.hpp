#pragma once

#ifndef polymer_editor_gizmo_controller_hpp
#define polymer_editor_gizmo_controller_hpp

#include "polymer-app-base/wrappers/gl-gizmo.hpp"
#include "polymer-app-base/wrappers/gl-imgui.hpp"
#include "polymer-engine/scene.hpp"
#include "polymer-engine/object.hpp"
#include "polymer-engine/ecs/core-ecs.hpp"
#include "polymer-app-base/ui-actions.hpp"

using namespace polymer;

class gizmo_controller
{
    gl_gizmo gizmo;
    tinygizmo::rigid_transform gizmo_transform; // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform previous_gizmo_transform;

    transform entity_transform;
    std::vector<entity> selected_entities;       // Array of selected objects
    std::vector<transform> relative_transforms;  // Pose of the objects relative to the selection

    app_input_event last_event;
    bool stopped_dragging{ false };
    bool started_dragging{ false };
    simple_cpu_timer cooldown_timer;
    bool gizmo_active{ false };
    bool move_flag{ false };

    scene * the_scene{ nullptr };

    // Helper to get world transform from entity
    transform get_world_transform(entity e)
    {
        base_object & obj = the_scene->get_graph().get_object(e);
        if (auto * xform = obj.get_component<transform_component>())
        {
            return xform->get_world_transform();
        }
        return {};
    }

    // Helper to get local scale from entity
    float3 get_local_scale(entity e)
    {
        base_object & obj = the_scene->get_graph().get_object(e);
        if (auto * xform = obj.get_component<transform_component>())
        {
            return xform->local_scale;
        }
        return float3(1.f);
    }

    // Helper to get parent entity
    entity get_parent(entity e)
    {
        return the_scene->get_graph().get_parent(e);
    }

    // Helper to set local transform
    void set_local_transform(entity e, const transform & pose, const float3 & scale)
    {
        base_object & obj = the_scene->get_graph().get_object(e);
        if (auto * xform = obj.get_component<transform_component>())
        {
            xform->local_pose = pose;
            xform->local_scale = scale;
        }
        the_scene->get_graph().refresh();
    }

    void compute_entity_transform()
    {
        // No selected objects? The selection pose is nil
        if (selected_entities.size() == 0)
        {
            entity_transform = {};
        }
        // Single object selection
        else if (selected_entities.size() == 1)
        {
            entity_transform = get_world_transform(selected_entities[0]);
        }
        // Multi-object selection
        else
        {
            float3 center_of_mass = {};
            float numObjects = 0;
            for (auto ent : selected_entities)
            {
                center_of_mass += get_world_transform(ent).position;
                numObjects++;
            }
            center_of_mass /= numObjects;
            entity_transform.position = center_of_mass;
        }

        compute_relative_transforms();

        // Gizmo location is now at the location of the entity in world space. We check
        // for changes in gizmo location to see if there's been any user interaction, so
        // we also set the previous transform.
        gizmo_transform = from_linalg(entity_transform);
        previous_gizmo_transform = gizmo_transform;
    }

    void compute_relative_transforms()
    {
        relative_transforms.clear();

        for (entity e : selected_entities)
        {
            const transform x = get_world_transform(e);
            relative_transforms.push_back(entity_transform.inverse() * x);
        }
    }

public:

    gizmo_controller(scene * the_scene) : the_scene(the_scene) {}

    bool selected(entity e) const
    {
        return std::find(selected_entities.begin(), selected_entities.end(), e) != selected_entities.end();
    }

    std::vector<entity> get_selection()
    {
        return (selected_entities.size() ? selected_entities : std::vector<entity>{});
    }

    void set_selection(const std::vector<entity> & new_selection)
    {
        selected_entities = new_selection;
        compute_entity_transform();
    }

    void update_selection(entity object)
    {
        auto it = std::find(std::begin(selected_entities), std::end(selected_entities), object);
        if (it == std::end(selected_entities)) selected_entities.push_back(object);
        else selected_entities.erase(it);
        compute_entity_transform();
    }

    void clear()
    {
        selected_entities.clear();
        compute_entity_transform();
    }

    void refresh() { compute_entity_transform(); }
    bool active() const { return gizmo_active; }

    bool moved() 
    { 
        if (move_flag)
        {
            move_flag = false;
            return true;
        }
        return false; 
    }

    void on_input(const app_input_event & event)
    {
        gizmo.handle_input(event);
        if (event.drag == false && last_event.drag == true) stopped_dragging = true;
        if (event.drag == true && last_event.drag == false) started_dragging = true;
        last_event = event;
    }

    void reset_input()
    {
        gizmo.reset_input();
    }

    void on_update(const perspective_camera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_transform);

        // Has the gizmo moved?
        if (gizmo_transform != previous_gizmo_transform)
        {
            gizmo_active = true;
            move_flag = true;

            // For each selected entity...
            for (int i = 0; i < selected_entities.size(); ++i)
            {
                const entity e = selected_entities[i];
                const transform updated_pose = to_linalg(gizmo_transform) * relative_transforms[i];
                const float3 local_scale = get_local_scale(e);

                // Does this have a parent?
                if (get_parent(e) != kInvalidEntity)
                {
                    // `updated_pose` is in worldspace, even though it's a child.
                    // We need to bring it back into the space of the parent.
                    const entity parent_entity = get_parent(e);
                    base_object & parent_obj = the_scene->get_graph().get_object(parent_entity);
                    transform_component * parent_xform = parent_obj.get_component<transform_component>();
                    const transform parent_pose = parent_xform ? parent_xform->local_pose : transform();
                    const transform child_local_pose = parent_pose.inverse() * updated_pose;

                    set_local_transform(e, child_local_pose, local_scale);
                }
                else
                {
                    // Note how we're setting the local transform. If this is a parent entity,
                    // local is already in worldspace.
                    set_local_transform(e, updated_pose, local_scale);
                }
            }
            previous_gizmo_transform = gizmo_transform;
        }

        // Finished the editing action
        if (stopped_dragging == true)
        {
            stopped_dragging = false;
            cooldown_timer.reset();
            cooldown_timer.start();
        }
        else if (last_event.drag == false)
        {
            gizmo_active = false;
        }

        if (cooldown_timer.milliseconds().count() > 0.f && cooldown_timer.milliseconds().count() <= 250)
        {
            gizmo_active = true; // keep the gizmo active
        }
    }

    void on_draw(const float screenspace_scale = 0.0f)
    {
        if (selected_entities.size())
        {
            gizmo.draw(screenspace_scale);
        }
    }
};

#endif // end polymer_editor_gizmo_controller_hpp
