#pragma once

#ifndef editor_selection_hpp
#define editor_selection_hpp

#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "environment.hpp"
#include "ecs/core-ecs.hpp"
#include "system-transform.hpp"

class selection_controller
{
    gl_gizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection; // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform last_gizmo_selection;

    transform selection;
    std::vector<entity> selected_entities;       // Array of selected objects
    std::vector<transform> relative_transforms;  // Pose of the objects relative to the selection

    bool gizmo_active{ false };
    transform_system * xform_system{ nullptr };

    void compute_selection()
    {
        // No selected objects? The selection pose is nil
        if (selected_entities.size() == 0)
        {
            selection = {};
        }
        // Single object selection
        else if (selected_entities.size() == 1)
        {
            selection = xform_system->get_world_transform(selected_entities[0])->world_pose;
        }
        // Multi-object selection
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            float numObjects = 0;
            for (auto entity : selected_entities)
            {
                center_of_mass += xform_system->get_world_transform(entity)->world_pose.position;
                numObjects++;
            }
            center_of_mass /= numObjects;
            selection.position = center_of_mass;
        }

        compute_relative_transforms();

        gizmo_selection = from_linalg(selection);

    }

    void compute_relative_transforms()
    {
        relative_transforms.clear();

        for (entity e : selected_entities)
        {
            const transform x = xform_system->get_world_transform(e)->world_pose;
            relative_transforms.push_back(selection.inverse() * x);
        }
    }

public:

    selection_controller(transform_system * system) : xform_system(system) { }

    bool selected(entity object) const
    {
        return std::find(selected_entities.begin(), selected_entities.end(), object) != selected_entities.end();
    }

    std::vector<entity> get_selection()
    {
        return selected_entities;
    }

    void set_selection(const std::vector<entity> & new_selection)
    {
        selected_entities = new_selection;
        compute_selection();
    }

    void update_selection(entity object)
    {
        auto it = std::find(std::begin(selected_entities), std::end(selected_entities), object);
        if (it == std::end(selected_entities)) selected_entities.push_back(object);
        else selected_entities.erase(it);
        compute_selection();
    }

    void clear()
    {
        selected_entities.clear();
        compute_selection();
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

    void on_update(const perspective_camera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        gizmo_active = tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);

        // Perform editing updates on selected objects
        if (gizmo_selection != last_gizmo_selection)
        {
            for (int i = 0; i < selected_entities.size(); ++i)
            {
                const entity object = selected_entities[i];
                const transform updated_pose = to_linalg(gizmo_selection) * relative_transforms[i];
                xform_system->set_local_transform(object, updated_pose);
            }
        }

        last_gizmo_selection = gizmo_selection;
    }

    void on_draw()
    {
        if (selected_entities.size())
        {
            gizmo.draw();
        }
    }
};

#endif
