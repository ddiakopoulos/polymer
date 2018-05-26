#pragma once

#ifndef polymer_gl_particle_system_hpp
#define polymer_gl_particle_system_hpp

#include "gl-api.hpp"
#include "util.hpp"

namespace polymer
{

    //////////////////
    //   Particle   //
    //////////////////

    struct particle
    {
        float3 position;
        float3 velocity;
        float size;
        float lifeMs;
        bool isDead{ false };
    };

    ////////////////////////////
    //   Particle Modifiers   //
    ////////////////////////////

    struct particle_modifier
    {
        virtual void update(std::vector<particle> & particles, float dt) = 0;
    };

    struct gravity_modifier final : public particle_modifier
    {
        float3 gravityVec;
        gravity_modifier(const float3 gravity) : gravityVec(gravity) {}
        void update(std::vector<particle> & particles, float dt) override
        {
            for (auto & p : particles)
            {
                p.velocity += gravityVec * dt;
            }
        }
    };

    struct point_gravity_modifier final : public particle_modifier
    {
        float3 position;
        float strength;
        float maxStrength;
        float radiusSquared;

        point_gravity_modifier(float3 & position, float strength, float maxStrength, float radius)
            : position(position), strength(strength), maxStrength(maxStrength), radiusSquared(radius * radius) {}

        void update(std::vector<particle> & particles, float dt) override
        {
            for (auto & p : particles)
            {
                const float3 distance = position - p.position;
                const float distSqr = length2(distance);
                if (distSqr > radiusSquared) return;
                float force = strength / distSqr;
                force = force > maxStrength ? maxStrength : force;
                p.velocity += normalize(distance) * force;
            }
        }
    };

    struct damping_modifier final : public particle_modifier
    {
        float damping;
        damping_modifier(const float damping) : damping(damping) { }
        void update(std::vector<particle> & particles, float dt) override
        {
            for (auto & p : particles)
            {
                p.velocity *= std::pow(damping, dt);
            }
        }
    };

    struct ground_modifier final : public particle_modifier
    {
        plane ground;
        ground_modifier(const plane p) : ground(p) {}
        void update(std::vector<particle> & particles, float dt) override
        {
            for (auto & p : particles)
            {
                const float reflectedVelocity = dot(ground.get_normal(), p.velocity);
                if (dot(ground.equation, float4(p.position, 1)) < 0.f && reflectedVelocity < 0.f)
                {
                    p.velocity -= ground.get_normal() * (reflectedVelocity * 2.0f);
                }
            }
        }
    };

    /////////////////////////////
    //   CPU Particle System   //
    /////////////////////////////

    class gl_particle_system
    {
        std::vector<particle> particles;
        std::vector<float4> instances;
        gl_buffer vertexBuffer, instanceBuffer;
        gl_vertex_array_object vao;
        std::vector<std::shared_ptr<particle_modifier>> particleModifiers;
        size_t trail{ 0 };
    public:
        gl_particle_system(size_t trail_count);
        void update(const float dt, const float3 gravityVec);
        void add_modifier(std::shared_ptr<particle_modifier> modifier);
        void add(const float3 position, const float3 velocity, const float size, const float lifeMs);
        void draw(const float4x4 & viewMat, const float4x4 & projMat, gl_shader & shader, gl_texture_2d & particle_tex, const float time);
        std::vector<particle> & get() { return particles; }
    };

    ///////////////////////////
    //   Particle Emitters   //
    ///////////////////////////

    struct particle_emitter
    {
        transform pose;
        uniform_random_gen gen;
        virtual void emit(gl_particle_system & system) = 0;
    };

    struct point_emitter final : public particle_emitter
    {
        void emit(gl_particle_system & system) override
        {
            for (int i = 0; i < 4; ++i)
            {
                const auto v1 = gen.random_float(-.5f, +.5f);
                const auto v2 = gen.random_float(0.5f, 2.f);
                const auto v3 = gen.random_float(-.5f, +.5f);
                system.add(pose.position, float3(v1, v2, v3), gen.random_float(0.05f, 0.2f), 2.5f);
            }
        }
    };

    struct cube_emitter final : public particle_emitter
    {
        aabb_3d localBounds;
        cube_emitter(aabb_3d local) : localBounds(local) {}
        void emit(gl_particle_system & system) override
        {
            const float3 min = pose.transform_coord(-(localBounds.size() * 0.5f));
            const float3 max = pose.transform_coord(+(localBounds.size() * 0.5f));

            for (int i = 0; i < 1; ++i)
            {
                const auto v1 = gen.random_float(min.x, max.x);
                const auto v2 = gen.random_float(min.y, max.y);
                const auto v3 = gen.random_float(min.z, max.z);
                system.add(float3(v1, v2, v3), float3(0, 1, 0), gen.random_float(0.05f, 0.2f), 4.f);
            }
        }
    };

    struct sphere_emitter final : public particle_emitter
    {
        aabb_3d localBounds;
        sphere_emitter(aabb_3d local) : localBounds(local) {}
        void emit(gl_particle_system & system) override
        {
            for (int i = 0; i < 12; ++i)
            {
                const float u = gen.random_float(0, 1) * float(POLYMER_PI);
                const float v = gen.random_float(0, 1) * float(POLYMER_TAU);
                const float3 normal = cartsesian_coord(u, v, 1.f);
                const float3 point = pose.transform_coord(normal);
                system.add(point, normal * 0.5f, 0.1f, 4.f);
            }
        }
    };

    struct quad_emitter final : public particle_emitter
    {
        aabb_2d localBounds;
        quad_emitter(aabb_2d local) : localBounds(local) {}
        void emit(gl_particle_system & system) override
        {
            for (int i = 0; i < 3; ++i)
            {
                const float2 halfExtents = localBounds.size() * 0.5f;
                const float w = gen.random_float(-halfExtents.x, halfExtents.x);
                const float h = gen.random_float(-halfExtents.y, halfExtents.y);
                const float3 point = pose.transform_coord(float3(w, 0, h));
                system.add(point, float3(0, 1, 0), 0.1f, 4.f);
            }
        }
    };

    struct disc_emitter final : public particle_emitter
    {
        aabb_2d localBounds;
        disc_emitter(aabb_2d local) : localBounds(local) {}
        void emit(gl_particle_system & system) override
        {
            const float2 size = localBounds.size();
            float radius = 0.5f * std::sqrt(size.x * size.x + size.y * size.y);
            radius = gen.random_float(0, radius);
            for (int i = 0; i < 3; ++i)
            {
                const float ang = gen.random_float_sphere();
                const float w = std::cos(ang) * radius;
                const float h = std::sin(ang) * radius;
                const float3 point = pose.transform_coord(float3(w, 0, h));
                system.add(point, float3(0, 1, 0), 0.1f, 4.f);
            }
        }
    };

} // end namespace polymer

#endif // end polymer_gl_particle_system_hpp
