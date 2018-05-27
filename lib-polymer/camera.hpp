#ifndef polymer_camera_hpp
#define polymer_camera_hpp

#include "math-core.hpp"
#include "util.hpp"
#include "geometry.hpp"

namespace polymer
{
    ////////////////////////////
    //   perspective_camera   //
    ////////////////////////////

    struct perspective_camera
    {
        transform pose;

        float vfov{ 1.3f };
        float nearclip{ 0.01f };
        float farclip{ 64.f };

        float4x4 get_view_matrix() const { return pose.view_matrix(); }
        float4x4 get_projection_matrix(float aspectRatio) const { return make_projection_matrix(vfov, aspectRatio, nearclip, farclip); }

        float3 get_view_direction() const { return -pose.zdir(); }
        float3 get_eye_point() const { return pose.position; }

        void look_at(const float3 & target) { pose = lookat_rh(pose.position, target); }
        void look_at(const float3 & eyePoint, const float3 target) { pose = lookat_rh(eyePoint, target); }
        void look_at(const float3 & eyePoint, float3 const & target, float3 const & worldup) { pose = lookat_rh(eyePoint, target, worldup); }

        ray get_world_ray(const float2 cursor, const float2 viewport) const
        {
            const float aspect = viewport.x / viewport.y;
            const ray r = ray_from_viewport_pixel(cursor, viewport, get_projection_matrix(aspect));
            return pose * r;
        }
    };

    /////////////////////////////////////
    //   Standard Free-Flying Camera   //
    /////////////////////////////////////

    class fps_camera_controller
    {
        perspective_camera * cam;

        float camPitch = 0, camYaw = 0;

        bool bf = 0, bl = 0, bb = 0, br = 0, ml = 0, mr = 0;
        float2 lastCursor;

    public:

        bool enableSpring = true;
        float movementSpeed = 14.0f;
        float3 velocity;

        fps_camera_controller() {}

        fps_camera_controller(perspective_camera * cam) : cam(cam)
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
            const float3 worldNorth = { 0, 0, -1 };
            float3 lookVec = cam->get_view_direction();
            float3 flatLookVec = safe_normalize(float3(lookVec.x, 0, lookVec.z));
            camYaw = std::acos(clamp(dot(worldNorth, flatLookVec), -1.0f, +1.0f)) * (flatLookVec.x > 0 ? -1 : 1);
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
                    camYaw -= (e.cursor.x - lastCursor.x) * 0.01f;
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

            if (bf || (ml && mr))
            {
                move.z -= 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bl)
            {
                move.x -= 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bb)
            {
                move.z += 1 * instantaneousSpeed;
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
            else cam->pose.position = target;

            float3 lookVec;
            lookVec.x = cam->get_eye_point().x - 1.f * cosf(camPitch) * sinf(camYaw);
            lookVec.y = cam->get_eye_point().y + 1.f * sinf(camPitch);
            lookVec.z = cam->get_eye_point().z - 1.f * cosf(camPitch) * cosf(camYaw);
            cam->look_at(lookVec);
        }
    };

} // end namespace polymer

#endif // end polymer_camera_hpp
