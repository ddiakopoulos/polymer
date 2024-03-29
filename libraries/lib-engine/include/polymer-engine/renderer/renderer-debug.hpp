#pragma once

#ifndef polymer_renderer_debug_hpp
#define polymer_renderer_debug_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/procedural-mesh.hpp"

#include "polymer-gfx-gl/gl-api.hpp"

#include "polymer-engine/scene.hpp"

// todo: refactor to inline shaders and make debug renderer a gl-component rather than being 
// included in polymer-engine directly

namespace polymer
{
    class global_debug_mesh_manager : public singleton<global_debug_mesh_manager>
    {
        friend class polymer::singleton<global_debug_mesh_manager>;

        scene * the_scene{ nullptr };

        struct Vertex { float3 position; float3 color; };
        std::vector<Vertex> vertices;

        entity dbg_renderer_ent{ kInvalidEntity };
        std::shared_ptr<polymer_procedural_material> debug_renderer_material;

    public:

        global_debug_mesh_manager() = default;


        void initialize_resources(entity_system_manager * esm, scene * the_scene)
        {
            debug_renderer_material = std::make_shared<polymer_procedural_material>();
            debug_renderer_material->shader = shader_handle("debug-renderer");
            debug_renderer_material->cast_shadows = false;
            the_scene->mat_library->register_material("debug-renderer-material", debug_renderer_material);

            dbg_renderer_ent = the_scene->track_entity(esm->create_entity());
            the_scene->identifier_system->create(dbg_renderer_ent, "debug_renderer-" + std::to_string(dbg_renderer_ent));
            the_scene->xform_system->create(dbg_renderer_ent, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

            auto mat_c = material_component(dbg_renderer_ent, material_handle("debug-renderer-material"));
            mat_c.cast_shadow = false;
            mat_c.receive_shadow = false;
            the_scene->render_system->create(dbg_renderer_ent, std::move(mat_c));
            the_scene->render_system->create(dbg_renderer_ent, mesh_component(dbg_renderer_ent, gpu_mesh_handle("debug-renderer")));
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
            auto unit_cube = make_cube(local_bounds.size() * 0.5f);

            for (auto & v : unit_cube.vertices)
            {
                //v *= ; // / 2.f; // scale
                auto tV = local_to_world.transform_coord(v); // translate
                vertices.push_back({ tV, color });
            }
        }

        void draw_sphere(const transform & local_to_world, const float scale = 1.f, const float3 color = float3(1, 1, 1))
        {
            auto make_wireframe_sphere = [this, scale, local_to_world]()
            {
                geometry g;
                const float dr = to_radians(360.0f / 90.f);

                for (float r = 0.f; r < POLYMER_TAU; r += dr) 
                {
                    float sin_r0 = std::sin(r);
                    float cos_r0 = std::cos(r);
                    float sin_r1 = std::sin(r + dr);
                    float cos_r1 = std::cos(r + dr);

                    float3 v0_x(0.0f, sin_r0 * scale, cos_r0 * scale);
                    float3 v1_x(0.0f, sin_r1 * scale, cos_r1 * scale);
                    float3 v0_y(sin_r0 * scale, 0.0f, cos_r0 * scale);
                    float3 v1_y(sin_r1 * scale, 0.0f, cos_r1 * scale);
                    float3 v0_z(sin_r0 * scale, cos_r0 * scale, 0.0f);
                    float3 v1_z(sin_r1 * scale, cos_r1 * scale, 0.0f);

                    g.vertices.push_back(float3(v0_x.x, v0_x.y, v0_x.z)); 
                    g.vertices.push_back(float3(v1_x.x, v1_x.y, v1_x.z));
                    g.vertices.push_back(float3(v0_y.x, v0_y.y, v0_y.z)); 
                    g.vertices.push_back(float3(v1_y.x, v1_y.y, v1_y.z));
                    g.vertices.push_back(float3(v0_z.x, v0_z.y, v0_z.z)); 
                    g.vertices.push_back(float3(v1_z.x, v1_z.y, v1_z.z));
                }

                return g;
            };

            auto unit_sphere = make_wireframe_sphere();

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

        void draw_obb()
        {
            // todo
        }

        void draw_frustum()
        {
            // todo
        }

        void draw_text()
        {
            // todo
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
