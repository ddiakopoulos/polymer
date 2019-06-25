#pragma once

#ifndef polymer_renderer_debug_hpp
#define polymer_renderer_debug_hpp

#include "math-core.hpp"
#include "gl-api.hpp"
#include "procedural_mesh.hpp"
#include "environment.hpp"

namespace polymer
{
    class global_debug_mesh_manager : public singleton<global_debug_mesh_manager>
    {
        friend class polymer::singleton<global_debug_mesh_manager>;

        environment * env{ nullptr };

        struct Vertex { float3 position; float3 color; };
        std::vector<Vertex> vertices;

        entity dbg_renderer_ent;
        std::shared_ptr<polymer_procedural_material> debug_renderer_material;

    public:

        global_debug_mesh_manager() = default;

        void initialize_resources(entity_orchestrator * orch, environment * env)
        {
            debug_renderer_material = std::make_shared<polymer_procedural_material>();
            debug_renderer_material->shader = shader_handle("debug-renderer");
            debug_renderer_material->cast_shadows = false;
            env->mat_library->register_material("debug-renderer-material", debug_renderer_material);

            dbg_renderer_ent = env->track_entity(orch->create_entity());
            env->identifier_system->create(dbg_renderer_ent, "debug_renderer-" + std::to_string(dbg_renderer_ent));
            env->xform_system->create(dbg_renderer_ent, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

            auto mat_c = material_component(dbg_renderer_ent, material_handle("debug-renderer-material"));
            mat_c.cast_shadow = false;
            mat_c.receive_shadow = false;
            env->render_system->create(dbg_renderer_ent, std::move(mat_c));
            env->render_system->create(dbg_renderer_ent, mesh_component(dbg_renderer_ent, gpu_mesh_handle("debug-renderer")));
        }

        void clear() { vertices.clear(); }

        // World-space
        void draw_line(const float3 & world_from, const float3 & world_to, const float3 color = float3(1, 1, 1))
        {
            vertices.push_back({ world_from, color });
            vertices.push_back({ world_to, color });
        }

        void draw_line(const transform & local_to_world, const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
        {
            vertices.push_back({ local_to_world.transform_coord(from), color });
            vertices.push_back({ local_to_world.transform_coord(to), color });
        }

        void draw_box(const transform & local_to_world, const aabb_3d & local_bounds, const float3 color = float3(1, 1, 1))
        {
            auto unit_cube = make_cube();

            for (auto & v : unit_cube.vertices)
            {
                v *= local_bounds.size() / 2.f; // scale
                auto tV = local_to_world.transform_coord(v); // translate
                vertices.push_back({ tV, color });
            }
        }

        void draw_sphere(const transform & local_to_world, const float scale = 1.f, const float3 color = float3(1, 1, 1))
        {
            auto unit_sphere = make_sphere(scale);

            for (auto & v : unit_sphere.vertices)
            {
                auto tV = local_to_world.transform_coord(v);
                vertices.push_back({ tV, color });
            }
        }

        void draw_axis(const transform & local_to_world, const float3 scale = float3(1, 1, 1))
        {
            auto axis = make_axis();

            for (int i = 0; i < axis.vertices.size(); ++i)
            {
                auto v = axis.vertices[i] * scale;
                v = local_to_world.transform_coord(v);
                vertices.push_back({ v, axis.colors[i].xyz });
            }
        }

        void upload()
        {
            auto & debug_renderer_mesh = gpu_mesh_handle("debug-renderer").get();
            debug_renderer_mesh.set_vertices(vertices.size(), vertices.data(), GL_STREAM_DRAW);
            debug_renderer_mesh.set_attribute(0, &Vertex::position);
            debug_renderer_mesh.set_attribute(2, &Vertex::color);
            debug_renderer_mesh.set_non_indexed(GL_LINES);
        }

        entity get_entity() const { return dbg_renderer_ent; }
    };

    // Implement singleton
    template<> global_debug_mesh_manager * polymer::singleton<global_debug_mesh_manager>::single = nullptr;

} // end namespace polymer

#endif // end polymer_renderer_debug_hpp
