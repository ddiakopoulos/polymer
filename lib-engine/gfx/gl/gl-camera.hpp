#ifndef camera_h
#define camera_h

#include "gl-api.hpp"
#include "math-core.hpp"
#include "camera.hpp"

#include "stb/stb_image_write.h"

namespace polymer
{


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
            const float3 worldNorth = {0, 0, -1};
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
            else
            {
                cam->pose.position = target;
            }
            
            float3 lookVec;
            lookVec.x = cam->get_eye_point().x - 1.f * cosf(camPitch) * sinf(camYaw);
            lookVec.y = cam->get_eye_point().y + 1.f * sinf(camPitch);
            lookVec.z = cam->get_eye_point().z - 1.f * cosf(camPitch) * cosf(camYaw);
            cam->look_at(lookVec);
        }
    };
    
    ////////////////////////////
    //   gl_cubemap_capture   //
    ////////////////////////////

    class gl_cubemap_capture
    {
        gl_framebuffer framebuffer;
        gl_texture_2d cubeMapColor;
        gl_texture_2d cubeMapDepth;

        float resolution;
        bool shouldCapture = false;

        void save_pngs()
        {
            const std::vector<std::string> faceNames = {{"positive_x"}, {"negative_x"}, {"positive_y"}, {"negative_y"}, {"positive_z"}, {"negative_z"}};
            std::vector<uint8_t> data(static_cast<size_t>(resolution * resolution * 3));
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapColor);
            for (int i = 0; i < 6; ++i)
            {
                glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
                stbi_write_png( std::string(faceNames[i] + ".png").c_str(), static_cast<int>(resolution), static_cast<int>(resolution), 3, data.data(), static_cast<int>(resolution) * 3);
                gl_check_error(__FILE__, __LINE__);
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            shouldCapture = false;
        }

    public:

        std::function<void(float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)> render;

        gl_cubemap_capture(int resolution) : resolution(static_cast<float>(resolution))
        {
            cubeMapColor.setup_cube(resolution, resolution, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            cubeMapDepth.setup_cube(resolution, resolution, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            gl_check_error(__FILE__, __LINE__);
         }

         GLuint get_cubemap_handle() const { return cubeMapColor; }
     
         void export_pngs() { shouldCapture = true; }

         void update(const float3 worldLocation)
         {
             if (shouldCapture)
             {
                 GLint drawFboId = 0, readFboId = 0;
                 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
                 glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFboId);

                 glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
                 glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);

                 std::vector<float3> targets = { { 1, 0, 0, },{ -1, 0, 0 },{ 0, 1, 0 },{ 0, -1, 0 },{ 0, 0, 1 },{ 0, 0, -1 } };
                 std::vector<float3> upVecs = { { 0, -1, 0 },{ 0, -1, 0 },{ 0, 0, 1 },{ 0, 0, 1 },{ 0, -1, 0 },{ 0, -1, 0 } };
                 const float4x4 projMatrix = make_projection_matrix(to_radians(90.f), 1.0f, 0.1f, 128.f);

                 for (int i = 0; i < 6; ++i)
                 {
                     glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMapColor, 0);
                     glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMapDepth, 0);
                     glViewport(0, 0, static_cast<int>(resolution), static_cast<int>(resolution));
                     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                     const float4x4 viewMatrix = lookat_rh(worldLocation, targets[i], upVecs[i]).view_matrix();

                     if (render)
                     {
                         render(worldLocation, viewMatrix, projMatrix);
                     }
                 }
                 save_pngs();

                 glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);
                 glBindFramebuffer(GL_READ_FRAMEBUFFER, readFboId);
             }
        }
    };
}

#endif // camera_h
