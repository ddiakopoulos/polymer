#ifndef gl_gizmo_hpp
#define gl_gizmo_hpp

#include "util.hpp"
#include "gl-api.hpp"

#include "camera-controllers.hpp"
#include "tinygizmo/tiny-gizmo.hpp"

static inline polymer::transform to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<polymer::quatf &>(t.orientation), reinterpret_cast<polymer::float3 &>(t.position) };
}

static inline tinygizmo::rigid_transform from_linalg(polymer::transform & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

namespace polymer
{
    constexpr const char gl_gizmo_vert[] = R"(#version 330
        layout(location = 0) in vec3 vertex;
        layout(location = 1) in vec3 normal;
        layout(location = 2) in vec3 color;
        out vec3 v_color;
        uniform mat4 u_mvp;
        void main()
        {
            gl_Position = u_mvp * vec4(vertex.xyz, 1);
            v_color = color;
        }
    )";

    constexpr const char gl_gizmo_frag[] = R"(#version 330
        in vec3 v_color;
        out vec4 f_color;

        void main()
        {
            f_color = vec4(v_color, 1);
        }
    )";

    struct gl_gizmo : public non_copyable
    {
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;

        gl_shader program;
        gl_mesh mesh;

        float4x4 viewProjectionMatrix;
        float2 lastCursorPosition;

        gl_gizmo()
        {
            program = gl_shader(gl_gizmo_vert, gl_gizmo_frag);

            gizmo_ctx.render = [&](const  tinygizmo::geometry_mesh & r)
            {
                // Upload
                const std::vector<linalg::aliases::float3> & verts = reinterpret_cast<const std::vector<linalg::aliases::float3> &>(r.vertices);
                const std::vector<linalg::aliases::uint3> & tris = reinterpret_cast<const std::vector<linalg::aliases::uint3> &>(r.triangles);
                mesh.set_vertices(verts, GL_DYNAMIC_DRAW);
                mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, position));
                mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, normal));
                mesh.set_attribute(2, 3, GL_FLOAT, GL_FALSE, sizeof(tinygizmo::geometry_vertex), (GLvoid*)offsetof(tinygizmo::geometry_vertex, color));
                mesh.set_elements(tris, GL_DYNAMIC_DRAW);

                // Draw
                program.bind();
                program.uniform("u_mvp", viewProjectionMatrix);
                mesh.draw_elements();
                program.unbind();
            };
        }

        void handle_input(const app_input_event & e)
        {
            if (e.type == app_input_event::KEY)
            {
                if (e.value[0] == GLFW_KEY_LEFT_CONTROL) gizmo_state.hotkey_ctrl = e.using_control_key();
                if (e.value[0] == GLFW_KEY_L) gizmo_state.hotkey_local = e.is_down();
                else if (e.value[0] == GLFW_KEY_W) gizmo_state.hotkey_translate = e.is_down();
                else if (e.value[0] == GLFW_KEY_E) gizmo_state.hotkey_rotate = e.is_down();
                else if (e.value[0] == GLFW_KEY_R) gizmo_state.hotkey_scale = e.is_down();
            }
            if (e.type == app_input_event::MOUSE && e.value[0] == GLFW_MOUSE_BUTTON_LEFT) gizmo_state.mouse_left = e.is_down();
            lastCursorPosition = e.cursor;
        }

        void reset_input()
        {
            gizmo_state.ray_origin = { 0, 0, 0 };
            gizmo_state.ray_direction = { 0, 0, 0 };
            gizmo_state.mouse_left = false;
            gizmo_state.hotkey_ctrl = false;
            gizmo_state.hotkey_local = false;;
            gizmo_state.hotkey_translate = false;
            gizmo_state.hotkey_rotate = false;
            gizmo_state.hotkey_scale = false;
        }

        void update(const perspective_camera & cam, const polymer::float2 windowSize)
        {
            const transform p = cam.pose;
            const auto r = cam.get_world_ray(lastCursorPosition, windowSize);
            viewProjectionMatrix = (cam.get_projection_matrix(windowSize.x / windowSize.y) * cam.get_view_matrix());
            gizmo_state.viewport_size = minalg::float2(windowSize.x, windowSize.y);
            gizmo_state.cam.near_clip = cam.nearclip;
            gizmo_state.cam.far_clip = cam.farclip;
            gizmo_state.cam.yfov = cam.vfov;
            gizmo_state.cam.position = minalg::float3(p.position.x, p.position.y, p.position.z);
            gizmo_state.cam.orientation = minalg::float4(p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w);
            gizmo_state.ray_origin = minalg::float3(r.origin.x, r.origin.y, r.origin.z);
            gizmo_state.ray_direction = minalg::float3(r.direction.x, r.direction.y, r.direction.z);
            gizmo_ctx.update(gizmo_state);
        }

        void draw(const float screenspace_scale = 0.0f)
        {
            if (screenspace_scale > 0.f)
            {
                gizmo_state.screenspace_scale = screenspace_scale;
            }

            gizmo_ctx.draw();
        }

    };

} // end namespace polymer

#endif // gl_gizmo_hpp
