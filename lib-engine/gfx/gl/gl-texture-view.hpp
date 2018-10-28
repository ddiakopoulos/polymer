#ifndef texture_view_h
#define texture_view_h

#include "gl-api.hpp"
#include "gl-procedural-mesh.hpp"

static const char s_textureVert[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp;
    out vec2 texCoord;
    void main()
    {
        texCoord = uvs;
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
    }
)";

static const char s_textureVertFlip[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp;
    out vec2 texCoord;
    void main()
    {
        texCoord = vec2(uvs.x, 1 - uvs.y);
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
    }
)";

static const char s_textureFrag[] = R"(#version 330
    uniform sampler2D u_texture;
    in vec2 texCoord;
    out vec4 f_color;
    void main()
    {
        vec4 sample = texture(u_texture, texCoord);
        f_color = vec4(sample.rgb, 1.0); 
    }
)";

static const char s_textureFragDepth[] = R"(#version 330
    uniform sampler2D u_texture;
    uniform float u_zNear;
    uniform float u_zFar;

    in vec2 texCoord;
    out vec4 f_color;

    float linear_01_depth(in float z) 
    {
        // Used to linearize Z buffer values. x is (1-far/near), y is (far/near), z is (x/far) and w is (y/far).
        vec4 zBufferParams = vec4(1.0 - u_zFar/u_zNear, u_zFar/u_zNear, 0, 0);
        return (1.00000 / ((zBufferParams.x * z) + zBufferParams.y ));
    }

    void main()
    {
        vec4 sample = texture(u_texture, texCoord);
        float linearDepthSample = linear_01_depth(sample.r);
        f_color = vec4(linearDepthSample, linearDepthSample, linearDepthSample, 1.0); 
    }
)";

///////////////////////////////////////////////////////////////////////////////////////

static const char s_textureVert3D[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp = mat4(1.0);
    out vec2 v_texcoord;
    void main()
    {
        v_texcoord = uvs;
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    }
)";

static const char s_textureFrag3D[] = R"(#version 330
    uniform sampler2DArray u_texture;
    uniform int u_slice;
    in vec2 v_texcoord;
    out vec4 f_color;
    void main()
    {
        vec4 sample = texture(u_texture, vec3(v_texcoord, float(u_slice)));
        f_color = vec4(sample.r, sample.r, sample.r, 1.0); // temp hack for debugging
    }
)";

namespace polymer
{

    struct gl_texture_view_2d : public non_copyable
    {
        gl_shader program;
        gl_mesh mesh = make_fullscreen_quad_screenspace();
        bool hasDepth = false;
        float2 nearFarDepth;

        gl_texture_view_2d(bool flip = false, float2 nearFarDepth = float2(0, 0)) : nearFarDepth(nearFarDepth)
        {
            if (nearFarDepth.x > 0 || nearFarDepth.y > 0) hasDepth = true;

            if (flip)
            {
                if (hasDepth) program = gl_shader(s_textureVertFlip, s_textureFragDepth);
                else program = gl_shader(s_textureVertFlip, s_textureFrag);
            }
            else
            {
                if (hasDepth) program = gl_shader(s_textureVert, s_textureFragDepth);
                else program = gl_shader(s_textureVert, s_textureFrag);
            }
        }
        
        void draw(const aabb_2d & rect, const float2 windowSize, const GLuint tex)
        {
            const float4x4 projection = make_orthographic_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
            float4x4 model = make_scaling_matrix({ rect.width(), rect.height(), 0.f });
            model = (make_translation_matrix({ rect.min().x, rect.min().y, 0.f }) * model);
            program.bind();
            program.uniform("u_mvp", (projection * model));
            if (hasDepth)
            {
                program.uniform("u_zNear", nearFarDepth.x);
                program.uniform("u_zFar", nearFarDepth.y);
            }
            program.texture("u_texture", 0, tex, GL_TEXTURE_2D);
            mesh.draw_elements();
            program.unbind();
        }
        
    };
    
    class gl_texture_view_3d : public non_copyable
    {
        gl_shader program;
        gl_mesh mesh = make_fullscreen_quad_screenspace(); 
    public:
        gl_texture_view_3d() { program = gl_shader(s_textureVert3D, s_textureFrag3D); }
        void draw(const aabb_2d & rect, const float2 windowSize, const GLuint tex, const GLenum target, const int slice)
        {
            const float4x4 projection = make_orthographic_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
            float4x4 model = make_scaling_matrix({ rect.width(), rect.height(), 0.f });
            model = (make_translation_matrix({rect.min().x, rect.min().y, 0.f}) * model);
            program.bind();
            program.uniform("u_mvp", projection * model);
            program.uniform("u_slice", slice);
            program.texture("u_texture", 0, tex, target);
            mesh.draw_elements();
            program.unbind();
        }
    };

    struct simple_texture_view
    {
        gl_shader shader;
        gl_mesh fullscreen_quad_ndc;

        simple_texture_view()
        {
            static const char s_textureVert[] = R"(#version 330
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec2 uvs;
                uniform mat4 u_mvp = mat4(1);
                out vec2 texCoord;
                void main()
                {
                    texCoord = uvs;
                    gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
                }
            )";

            static const char s_textureFrag[] = R"(#version 330
                uniform sampler2D s_texture;
                in vec2 texCoord;
                out vec4 f_color;
                void main()
                {
                    vec4 sample = texture(s_texture, texCoord);
                    f_color = vec4(sample.rgb, 1.0);
                }
            )";

            shader = gl_shader(s_textureVert, s_textureFrag);

            struct Vertex { float3 position; float2 texcoord; };
            const float3 verts[6] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
            const float2 texcoords[6] = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
            const uint3 faces[2] = { { 0, 1, 2 },{ 3, 4, 5 } };
            std::vector<Vertex> vertices;
            for (int i = 0; i < 6; ++i) vertices.push_back({ verts[i], texcoords[i] });

            fullscreen_quad_ndc.set_vertices(vertices, GL_STATIC_DRAW);
            fullscreen_quad_ndc.set_attribute(0, &Vertex::position);
            fullscreen_quad_ndc.set_attribute(1, &Vertex::texcoord);
            fullscreen_quad_ndc.set_elements(faces, GL_STATIC_DRAW);
        }

        void draw(const GLuint texture_handle)
        {
            GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
            glDisable(GL_DEPTH_TEST);
            shader.bind();
            shader.texture("s_texture", 0, texture_handle, GL_TEXTURE_2D);
            fullscreen_quad_ndc.draw_elements();
            shader.unbind();
            if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
        }
    };
 
}

#endif // texture_view_h
