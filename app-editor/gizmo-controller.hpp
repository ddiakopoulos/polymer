#pragma once

#ifndef gizmo_controller_hpp
#define gizmo_controller_hpp

#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "environment.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"

class gizmo_controller
{
    gl_gizmo gizmo;
    tinygizmo::rigid_transform gizmo_transform; // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform previous_gizmo_transform;

    transform entity_transform;
    std::vector<entity> selected_entities;       // Array of selected objects
    std::vector<transform> relative_transforms;  // Pose of the objects relative to the selection

    bool gizmo_active{ false };
    transform_system * xform_system{ nullptr };

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
            entity_transform = xform_system->get_world_transform(selected_entities[0])->world_pose;
        }
        // Multi-object selection
        else
        {
            float3 center_of_mass = {};
            float numObjects = 0;
            for (auto entity : selected_entities)
            {
                center_of_mass += xform_system->get_world_transform(entity)->world_pose.position;
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
            const transform x = xform_system->get_world_transform(e)->world_pose;
            relative_transforms.push_back(entity_transform.inverse() * x);
        }
    }

public:

    gizmo_controller(transform_system * system) : xform_system(system) { }

    bool selected(entity e) const
    {
        return std::find(selected_entities.begin(), selected_entities.end(), e) != selected_entities.end();
    }

    std::vector<entity> get_selection()
    {
        return selected_entities;
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

    void refresh()
    {
        //compute_entity_transform();
    }

    bool active() const
    {
        return gizmo_active;
    }

    void on_input(const app_input_event & event)
    {
        gizmo.handle_input(event);
    }

    void reset_input()
    {
        gizmo.reset_input();
    }

    // Behavior:
    // works if moving parent
    // does not work if moving child. 
    // The gizmo lands in the new spot alright, but as soon
    // as I move it fucks up
    // It only fucks up if I move the parent... 

    void on_update(const perspective_camera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        gizmo_active = tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_transform);

        // Has the gizmo moved? 
        if (gizmo_active && (gizmo_transform != previous_gizmo_transform))
        {
            // For each selected entity... 
            for (int i = 0; i < selected_entities.size(); ++i)
            {
                const entity e = selected_entities[i];
                const transform updated_pose = to_linalg(gizmo_transform) * relative_transforms[i];

                // Does this have a parent?
                if (xform_system->get_parent(e) != kInvalidEntity)
                {
                    // `updated_pose` is in worldspace, even though it's a child. 
                    // We need to bring it back into the space of the parent. 
                    const entity parent_entity = xform_system->get_parent(e);
                    const transform parent_pose = xform_system->get_local_transform(parent_entity)->local_pose;
                    const transform child_local_pose = parent_pose.inverse() * updated_pose;
                    xform_system->set_local_transform(e, child_local_pose);
                }
                else
                {
                    // Note how we're setting the local transform. If this is a parent entity,
                    // local is already in worldspace. 
                    xform_system->set_local_transform(e, updated_pose);
                }

                // Update the location of the drawn gizmo?
                //compute_entity_transform();
                //gizmo_transform = from_linalg(entity_transform);
            }

            previous_gizmo_transform = gizmo_transform;
        }
    }

    void on_draw()
    {
        if (selected_entities.size())
        {
            gizmo.draw();
        }
    }
};

#endif // end gizmo_controller_hpp
