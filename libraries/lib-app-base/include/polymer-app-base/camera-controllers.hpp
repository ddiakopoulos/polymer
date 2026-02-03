#pragma once

#ifndef polymer_camera_controllers_hpp
#define polymer_camera_controllers_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/camera.hpp"

#include "polymer-app-base/glfw-app.hpp"

namespace polymer
{

    ///////////////////////////////
    //   camera_controller_fps   //
    ///////////////////////////////

    class camera_controller_fps
    {
        perspective_camera * cam;
        
        float camPitch = 0, camYaw = 0;
        
        bool bf = 0, bl = 0, bb = 0, br = 0, ml = 0, mr = 0;
        float2 lastCursor;
        
    public:

        bool enableSpring = true;
        bool leftHanded = false;  // Use LH convention (+Z forward) instead of RH (-Z forward)
        float movementSpeed = 14.0f;
        float3 velocity;

        camera_controller_fps() {}
        
        camera_controller_fps(perspective_camera * cam) : cam(cam)
        {
            update_yaw_pitch();
        }
        
        void set_camera(perspective_camera * cam)
        {
            this->cam = cam;
            update_yaw_pitch();
        }
        
        void update_yaw_pitch()
        {
            const float3 worldNorth = leftHanded ? float3{0, 0, 1} : float3{0, 0, -1};
            float3 lookVec = cam->get_view_direction();
            float3 flatLookVec = safe_normalize(float3(lookVec.x, 0, lookVec.z));
            float yawSign = leftHanded ? 1.f : -1.f;
            camYaw = std::acos(clamp(dot(worldNorth, flatLookVec), -1.0f, +1.0f)) * (flatLookVec.x > 0 ? yawSign : -yawSign);
            camPitch = std::acos(clamp(dot(lookVec, flatLookVec), -1.0f, +1.0f)) * (lookVec.y > 0 ? 1 : -1);
        }

        void reset()
        {
            bf = bl = bb = br = ml = mr = 0;
            lastCursor = { 0, 0 };
        }
        
        void handle_input(const app_input_event & e)
        {
            switch (e.type)
            {
            case app_input_event::KEY:
                switch (e.value[0])
                {
                    case GLFW_KEY_W: bf = e.is_down(); break;
                    case GLFW_KEY_A: bl = e.is_down(); break;
                    case GLFW_KEY_S: bb = e.is_down(); break;
                    case GLFW_KEY_D: br = e.is_down(); break;
                }
                break;
            case app_input_event::MOUSE:
                switch (e.value[0])
                {
                    case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_down(); break;
                    case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_down(); break;
                }
                break;
            case app_input_event::CURSOR:
                if (mr)
                {
                    float yawSign = leftHanded ? 1.f : -1.f;
                    camYaw += yawSign * (e.cursor.x - lastCursor.x) * 0.01f;
                    camPitch = clamp(camPitch - (e.cursor.y - lastCursor.y) * 0.01f, -1.57f, +1.57f);
                }
                break;
            }
            lastCursor = e.cursor;
        }

        void update(float delta)
        {
            float3 move;

            float instantaneousSpeed = movementSpeed;
            float zSign = leftHanded ? 1.f : -1.f;  // LH: +Z forward, RH: -Z forward

            if (bf || (ml && mr))
            {
                move.z += zSign * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bl)
            {
                move.x -= 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bb)
            {
                move.z -= zSign * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (br)
            {
                move.x += 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }

            float3 & current = cam->pose.position;
            const float3 target = cam->pose.transform_coord(move);

            if (enableSpring)
            {
                critically_damped_spring(delta, target.x, 1.f, instantaneousSpeed, current.x, velocity.x);
                critically_damped_spring(delta, target.y, 1.f, instantaneousSpeed, current.y, velocity.y);
                critically_damped_spring(delta, target.z, 1.f, instantaneousSpeed, current.z, velocity.z);
            }
            else
            {
                cam->pose.position = target;
            }

            float3 lookVec;
            lookVec.x = cam->get_eye_point().x - 1.f * cosf(camPitch) * sinf(camYaw);
            lookVec.y = cam->get_eye_point().y + 1.f * sinf(camPitch);
            lookVec.z = cam->get_eye_point().z + zSign * cosf(camPitch) * cosf(camYaw);
            cam->pose = leftHanded ? lookat_lh(cam->get_eye_point(), lookVec) : lookat_rh(cam->get_eye_point(), lookVec);
        }
    };

    /////////////////////////////////
    //   camera_controller_orbit   //
    /////////////////////////////////

    class camera_controller_orbit
    {
        linalg::aliases::float3 make_direction_vector(const float yaw, const float pitch) const
        {
            return { cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw) };
        }

        struct delta_state
        {
            float delta_zoom{ 0.f };
            float delta_pan_x{ 0.f };
            float delta_pan_y{ 0.f };
            float delta_yaw{ 0.f };
            float delta_pitch{ 0.f };
        };

        struct frame_rh
        {
            linalg::aliases::float3 zDir, xDir, yDir;
            frame_rh() = default;
            frame_rh(const linalg::aliases::float3 & eyePoint, const linalg::aliases::float3 & target, const linalg::aliases::float3 & worldUp = { 0,1,0 })
            {
                zDir = normalize(eyePoint - target);
                xDir = normalize(cross(worldUp, zDir));
                yDir = cross(zDir, xDir);
            }
        };

        float yaw{ 0.f };
        float pitch{ 0.f };
        frame_rh f;

        linalg::aliases::float3 eye{ 0, 3, 3 };
        linalg::aliases::float3 target{ 0, 0, 0 };

        bool ml = 0, mr = 0, mm = 0;
        linalg::aliases::float2 last_cursor{ 0.f, 0.f };

        float focus = 10.f;

        bool hasUpdatedInput{ false };

    public:

        float zoom_scale = 1.f;
        float pan_scale = 0.1f;
        float rotate_scale = 0.0025f;

        delta_state delta;
        float yfov{ 1.f }, near_clip{ 0.01f }, far_clip{ 512.f };

        camera_controller_orbit() { set_target(target); }

        void set_target(const linalg::aliases::float3 & new_target)
        {
            target = new_target;
            const linalg::aliases::float3 lookat = normalize(eye - target);
            pitch = asinf(lookat.y);
            yaw = acosf(lookat.x);
            f = frame_rh(eye, target);
            // @todo - set focus
        }

        void set_eye_position(const linalg::aliases::float3 & new_eye)
        {
            eye = new_eye;
            f = frame_rh(eye, target);
        }

        void handle_input(const app_input_event & e)
        {
            if (e.type == app_input_event::Type::SCROLL)
            {
                delta.delta_zoom = (-e.value[1]) * zoom_scale;
            }
            else if (e.type == app_input_event::MOUSE)
            {
                if (e.value[0] == GLFW_MOUSE_BUTTON_LEFT) ml = e.is_down();
                else if (e.value[0] == GLFW_MOUSE_BUTTON_RIGHT) mr = e.is_down();
                else if (e.value[0] == GLFW_MOUSE_BUTTON_MIDDLE) mm = e.is_down();
            }
            else if (e.type == app_input_event::CURSOR)
            {
                const linalg::aliases::float2 delta_cursor = e.cursor - last_cursor;

                if (mr)
                {
                    if (e.mods & GLFW_MOD_SHIFT)
                    {
                        delta.delta_pan_x = delta_cursor.x * pan_scale;
                        delta.delta_pan_y = delta_cursor.y * pan_scale;
                    }
                    else
                    {
                        delta.delta_yaw = delta_cursor.x * rotate_scale;
                        delta.delta_pitch = delta_cursor.y * rotate_scale;
                    }
                }

                if (mm)
                {
                    delta.delta_pan_x = delta_cursor.x * pan_scale;
                    delta.delta_pan_y = delta_cursor.y * pan_scale;
                }
                last_cursor = e.cursor;
            }

            update(0.0);

        }

        void update(const float timestep, const float speed = 1.f)
        {
            if (delta.delta_pan_x != 0.f ||
                delta.delta_pan_y != 0.f ||
                delta.delta_yaw != 0.f ||
                delta.delta_pitch != 0.f ||
                delta.delta_zoom != 0.f) hasUpdatedInput = true;
            else hasUpdatedInput = false;

            if (should_update())
            {
                // Zoom / Eye Distance
                focus += (delta.delta_zoom);
                focus = std::max(0.1f, std::min(focus, 1024.f));

                // Rotate
                {
                    yaw += delta.delta_yaw;
                    pitch += delta.delta_pitch;

                    pitch = clamp(pitch, ((float)-POLYMER_PI / 2.f) + 0.1f, ((float)POLYMER_PI / 2.f) - 0.1f);
                    yaw = std::fmod(yaw, (float)POLYMER_TAU);

                    const linalg::aliases::float3 lookvec = normalize(make_direction_vector(yaw, pitch));
                    const linalg::aliases::float3 eye_point = (lookvec * focus);

                    eye = eye_point + target;
                    f = frame_rh(eye, target);
                }

                // Pan
                {
                    const linalg::aliases::float3 local_y = -normalize(f.yDir);
                    const linalg::aliases::float3 flat_x = normalize(f.xDir * linalg::aliases::float3(1, 0, 1));
                    const linalg::aliases::float3 delta_pan_offset = (flat_x * -delta.delta_pan_x) + (local_y * -delta.delta_pan_y) * float3(0.25f);
                    target += delta_pan_offset;
                }

                // Reset delta state
                delta.delta_yaw = 0.f;
                delta.delta_pitch = 0.f;
                delta.delta_pan_x = 0.f;
                delta.delta_pan_y = 0.f;
                delta.delta_zoom = 0.f;
            }
        }

        bool should_update(const float threshhold = 1e-3) const
        {
            if (std::abs(delta.delta_yaw) > threshhold)   return true;
            if (std::abs(delta.delta_pitch) > threshhold) return true;
            if (std::abs(delta.delta_pan_x) > threshhold) return true;
            if (std::abs(delta.delta_pan_y) > threshhold) return true;
            if (std::abs(delta.delta_zoom) > threshhold)  return true;
            return false;
        }

        float3 get_target() const { return target; }

        void set_transform(const float4x4 & m)
        {
            /*
             * @todo 
             * eye = m.z.xyz();
             */
        }

        void set_yfov(float fov_radians)
        {
            yfov = fov_radians;
        }

        transform get_transform() const
        {
            transform t;
            t.position = eye;
            t.orientation = normalize(make_rotation_quat_from_rotation_matrix({ f.xDir, f.yDir, f.zDir }));
            return t;
        }

        float4x4 get_view_matrix() const
        {
            transform view = get_transform();
            return view.view_matrix();
        }

        float4x4 get_projection_matrix(const float aspect) const
        {
            return matrix_xform::perspective(yfov, aspect, near_clip, far_clip);
        }

        float4x4 get_viewproj_matrix(const float aspect) const
        {
            return get_projection_matrix(aspect) * get_view_matrix();
        }
    };
    
} // namespace polymer

#endif // end polymer_camera_controllers_hpp
