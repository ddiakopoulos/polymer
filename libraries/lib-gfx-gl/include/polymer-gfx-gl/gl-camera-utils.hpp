#ifndef polymer_gl_camera_utils
#define polymer_gl_camera_utils

#include "polymer-gfx-gl/gl-api.hpp"
#include "polymer-core/math/math-core.hpp"
#include "polymer-core/tools/camera.hpp"

#include "stb/stb_image_write.h"

namespace polymer
{
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

#endif // polymer_gl_camera_utils
