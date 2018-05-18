#pragma once

#ifndef polymer_bullet_debug_hpp
#define polymer_bullet_debug_hpp

#include "math-core.hpp"
#include "bullet_utils.hpp"
#include "gl-api.hpp"

#include "stb/stb_easy_font.h"
#include "btBulletCollisionCommon.h"

namespace polymer
{
    class physics_visualizer : public btIDebugDraw
    {
        constexpr const static char debugVertexShader[] = R"(#version 330
            layout(location = 0) in vec3 vertex;
            layout(location = 1) in vec3 color;
            uniform mat4 u_mvp;
            out vec3 outColor;
            void main()
            {
                gl_Position = u_mvp * vec4(vertex.xyz, 1);
                outColor = color;
            }
        )";

        constexpr static const char debugFragmentShader[] = R"(#version 330
            in vec3 outColor;
            out vec4 f_color;
            void main()
            {
                f_color = vec4(outColor.rgb, 1);
            }
        )";

        std::vector<std::pair<float3, std::string>> text;
        struct Vertex { float3 position; float3 color; };
        std::vector<Vertex> vertices;
        gl_mesh debugMesh;
        gl_shader debugShader;

        int debugMode = 0;
        bool hasNewInfo{ false };

    public:

        physics_visualizer()
        {
            debugShader = gl_shader(debugVertexShader, debugFragmentShader);
        }

        void draw(const float4x4 & viewProj)
        {
            debugMesh.set_vertices(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
            debugMesh.set_attribute(0, &Vertex::position);
            debugMesh.set_attribute(1, &Vertex::color);
            debugMesh.set_non_indexed(GL_LINES);

            debugShader.bind();
            debugShader.uniform("u_mvp", viewProj);
            debugMesh.draw_elements();
            debugShader.unbind();
        }

        void clear()
        {
            text.clear();
            vertices.clear();
        }

        void drawContactPoint(const btVector3 & pointOnB, const btVector3 & normalOnB, btScalar distance, int lifeTime, const btVector3 & color)
        {
            drawLine(pointOnB, pointOnB + normalOnB * distance, color);
        }

        void drawLine(const btVector3 & from, const btVector3 & to, const btVector3 & color)
        {
            vertices.push_back({ from_bt(from), from_bt(color) });
            vertices.push_back({ from_bt(to), from_bt(color) });
        }

        void draw3dText(const btVector3 & position, const char * textString)
        {
            text.emplace_back(from_bt(position), textString);
        }

        void reportErrorWarning(const char * warningString) { std::cout << "Bullet Warning: " << warningString << std::endl; }

        void setDebugMode(int debugMode) override { this->debugMode = debugMode; }

        int getDebugMode() const { return debugMode; }

        void toggleDebugFlag(const int flag)
        {
            if (debugMode & flag) debugMode = debugMode & (~flag);
            else debugMode |= flag;
        }

    };

} // end namespace polymer

#endif // end polymer_bullet_debug_hpp
