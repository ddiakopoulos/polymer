#pragma once

#ifndef editor_selection_hpp
#define editor_selection_hpp

#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"
#include "scene.hpp"

static inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

static inline tinygizmo::rigid_transform from_linalg(Pose & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

template<typename T>
class selection_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform last_gizmo_selection;

    Pose selection;
    std::vector<T *> selected_objects;     // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection

    bool gizmo_active = false;

    void compute_selection()
    {
        // No selected objects? The selection pose is nil
        if (selected_objects.size() == 0)
        {
            selection = {};
        }
        // Single object selection
        else if (selected_objects.size() == 1)
        {
            selection = (*selected_objects.begin())->get_pose();
        }
        // Multi-object selection
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            float numObjects = 0;
            for (auto & obj : selected_objects)
            {
                center_of_mass += obj->get_pose().position;
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

        for (auto & s : selected_objects)
        {
            relative_transforms.push_back(selection.inverse() * s->get_pose());
        }
    }

public:

    selection_controller() { }

    bool selected(T * object) const
    {
        return std::find(selected_objects.begin(), selected_objects.end(), object) != selected_objects.end();
    }

    std::vector<T *> & get_selection()
    {
        return selected_objects;
    }

    void set_selection(const std::vector<T *> & new_selection)
    {
        selected_objects = new_selection;
        compute_selection();
    }

    void update_selection(T * object)
    {
        auto it = std::find(std::begin(selected_objects), std::end(selected_objects), object);
        if (it == std::end(selected_objects)) selected_objects.push_back(object);
        else selected_objects.erase(it);
        compute_selection();
    }

    void clear()
    {
        selected_objects.clear();
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
            for (int i = 0; i < selected_objects.size(); ++i)
            {
                T * object = selected_objects[i];
                auto newPose = to_linalg(gizmo_selection) * relative_transforms[i];
                object->set_pose(newPose);
            }
        }

        last_gizmo_selection = gizmo_selection;
    }

    void on_draw()
    {
        if (selected_objects.size())
        {
            gizmo.draw();
        }
    }
};

#endif
